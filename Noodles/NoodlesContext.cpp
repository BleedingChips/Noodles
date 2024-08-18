module;

#include <cassert>

module NoodlesContext;
import PotatoFormat;

namespace Noodles
{

	bool DetectConflict(std::span<RWUniqueTypeID const> t1, std::span<RWUniqueTypeID const> t2)
	{
		auto ite1 = t1.begin();
		auto ite2 = t2.begin();

		while (ite1 != t1.end() && ite2 != t2.end())
		{
			auto re = *ite1->atomic_type <=> *ite2->atomic_type;
			if (re == std::strong_ordering::equal)
			{
				if (
					ite1->is_write
					|| ite2->is_write
					)
				{
					return true;
				}
				else
				{
					++ite2; ++ite1;
				}
			}
			else if (re == std::strong_ordering::less)
			{
				++ite1;
			}
			else
			{
				++ite2;
			}
		}
		return false;
	}

	bool ReadWriteMutex::IsConflict(ReadWriteMutex const& mutex) const
	{
		return DetectConflict(
			index.components_span.Slice(total_type_id),
			mutex.index.components_span.Slice(mutex.total_type_id)
		)
			
		|| DetectConflict(
			index.singleton_span.Slice(total_type_id),
			mutex.index.singleton_span.Slice(mutex.total_type_id)
		)

			|| DetectConflict(
				index.user_modify.Slice(total_type_id),
				mutex.index.user_modify.Slice(mutex.total_type_id)
			)
		;
	}

	void ReadWriteMutexGenerator::RegisterComponentMutex(std::span<RWUniqueTypeID const> ifs)
	{
		for(auto& ite : ifs)
		{
			if (ite.ignore_mutex)
				continue;
			auto spn = std::span(unique_ids).subspan(0, component_count);
			bool Find = false;
			for(auto& ite2 : spn)
			{
				if(*ite.atomic_type == *ite2.atomic_type)
				{
					ite2.is_write = ite2.is_write || ite.is_write;
					Find = true;
					break;
				}
			}
			if(!Find)
			{
				unique_ids.insert(unique_ids.begin() + component_count, ite);
				++component_count;
			}
		}
	}

	void ReadWriteMutexGenerator::RegisterSingletonMutex(std::span<RWUniqueTypeID const> ifs)
	{
		for (auto& ite : ifs)
		{
			if (ite.ignore_mutex)
				continue;
			auto spn = std::span(unique_ids).subspan(component_count);
			bool Find = false;
			for (auto& ite2 : spn)
			{
				if (*ite.atomic_type == *ite2.atomic_type)
				{
					ite2.is_write = ite2.is_write || ite.is_write;
					Find = true;
					break;
				}
			}
			if (!Find)
			{
				unique_ids.emplace_back(ite);
				++singleton_count;
			}
		}
	}

	void ReadWriteMutexGenerator::RegisterUserModifyMutex(std::span<RWUniqueTypeID const> ifs)
	{
		for (auto& ite : ifs)
		{
			auto spn = std::span(unique_ids).subspan(component_count + singleton_count);
			bool Find = false;
			for (auto& ite2 : spn)
			{
				if (*ite.atomic_type == *ite2.atomic_type)
				{
					ite2.is_write = ite2.is_write || ite.is_write;
					Find = true;
					break;
				}
			}
			if (!Find)
			{
				unique_ids.emplace_back(ite);
				++user_modify_count;
			}
		}
	}

	ReadWriteMutex ReadWriteMutexGenerator::GetMutex() const
	{
		return ReadWriteMutex{
				std::span(unique_ids),
			Potato::Misc::IndexSpan<>{0, component_count},
			Potato::Misc::IndexSpan<>{component_count, component_count + singleton_count},
			Potato::Misc::IndexSpan<>{component_count + singleton_count, component_count + singleton_count + user_modify_count}
		};
	}

	void SystemNode::TaskFlowNodeExecute(Potato::Task::TaskFlowContext& status)
	{
		Context* Tar = nullptr;

		{
			SubContextTaskFlow* flow = static_cast<SubContextTaskFlow*>(status.flow.GetPointer());
			std::lock_guard lg(flow->process_mutex);
			Tar = static_cast<Context*>(flow->parent_node.GetPointer());
		}

		ExecuteContext exe_context
		{
			*Tar,
			status.node_property.display_name
		};

		SystemNodeExecute(exe_context);
	}

	static Potato::Format::StaticFormatPattern<u8"{}:{}"> system_static_format_pattern;

	Potato::IR::Layout SystemDisplayName::GetSerializeLayout() const
	{
		Potato::Format::FormatWritter<char8_t> temp_writer;
		auto re = system_static_format_pattern.Format(temp_writer, group, name);
		assert(re);
		return Potato::IR::Layout::GetArray<char8_t>(temp_writer.GetWritedSize());
	}

	void SystemDisplayName::SerializeTo(std::span<std::byte> output) const
	{
		Potato::Format::FormatWritter<char8_t> temp_writer{
			std::span<char8_t>{reinterpret_cast<char8_t*>(output.data()), output.size()}
		};
		auto re = system_static_format_pattern.Format(temp_writer, group, name);
		assert(re);
	}
	SystemDisplayName SystemDisplayName::ReMap(std::span<std::byte> input) const
	{
		return {
			std::u8string_view{reinterpret_cast<char8_t*>(input.data() + sizeof(char8_t) * ( group.size() + 1)), name.size()},
			std::u8string_view{reinterpret_cast<char8_t*>(input.data()), group.size()}
		};
	}

	/*
	auto SystemNodeUserData::Create(SystemNodeProperty property, ReadWriteMutex mutex, std::pmr::memory_resource* resource)
		-> Ptr
	{
		auto t_layout = Potato::IR::Layout::Get<SystemNodeUserData>();
		Potato::Format::FormatWritter<char8_t> temp_writer;
		system_static_format_pattern.Format(temp_writer, property.property.group, property.property.name);
		auto layout_offset = Potato::IR::InsertLayoutCPP(t_layout, Potato::IR::Layout::GetArray<RWUniqueTypeID>(mutex.total_type_id.size()));
		auto str_offset = Potato::IR::InsertLayoutCPP(t_layout, Potato::IR::Layout::GetArray<char8_t>(temp_writer.GetWritedSize()));
		Potato::IR::FixLayoutCPP(t_layout);
		auto re = Potato::IR::MemoryResourceRecord::Allocate(resource, t_layout);
		if(re)
		{
			std::byte* ite = re.GetByte() + layout_offset;
			for(auto& ite2 : mutex.total_type_id)
			{
				new (ite) RWUniqueTypeID{ ite2 };
				ite += sizeof(RWUniqueTypeID);
			}
			Potato::Format::FormatWritter<char8_t> temp_writer2{
				std::span(reinterpret_cast<char8_t*>(re.GetByte() + str_offset), temp_writer.GetWritedSize())
			};
			ReadWriteMutex new_mutex{
				std::span(reinterpret_cast<RWUniqueTypeID*>(re.GetByte() + layout_offset), mutex.total_type_id.size()),
				mutex.components_span,
				mutex.singleton_span,
				mutex.user_modify
			};
			system_static_format_pattern.Format(temp_writer2, property.property.group, property.property.name);
			property.property.group = { reinterpret_cast<char8_t*>(re.GetByte() + str_offset),  property.property.group.size()};
			property.property.name = { reinterpret_cast<char8_t*>(re.GetByte() + str_offset) + property.property.group.size() + 1,  property.property.name.size()};

			Ptr ptr = new (re.Get()) SystemNodeUserData{re, property, new_mutex, std::u8string_view
			{
				reinterpret_cast<char8_t*>(re.GetByte() + str_offset), temp_writer.GetWritedSize()
			} };
			return ptr;
		}
		return {};
	}
	

	void SystemNodeUserData::Release()
	{
		auto re = record;
		auto mu = mutex;
		this->~SystemNodeUserData();
		for(auto& ite : mu.total_type_id)
		{
			ite.~RWUniqueTypeID();
		}
		re.Deallocate();
	}
	*/

	bool SubContextTaskFlow::AddTickedNode(SystemNode& node, SystemNodeProperty property)
	{
		ReadWriteMutexGenerator gener{std::pmr::get_default_resource()};
		node.FlushMutexGenerator(gener);
		auto mutex = gener.GetMutex();
		std::lock_guard lg(preprocess_mutex);

		Potato::Task::TaskFlowNodeProperty nproperty
		{
			node.GetDisplayName().name,
			property.filter
		};

		if(AddNode_AssumedLocked(static_cast<Potato::Task::TaskFlowNode&>(node), nproperty))
		{
			std::size_t old_size = read_write_ids.size();
			read_write_ids.insert(read_write_ids.end(), 
				std::move_iterator(gener.unique_ids.begin()), 
				std::move_iterator(gener.unique_ids.end())
			);
			mutex.total_type_id = std::span(read_write_ids).subspan(old_size);
			std::size_t index = 0;
			auto disname = node.GetDisplayName();
			for(auto& ite : system_mutex)
			{
				ReadWriteMutex ite_mutex{std::span(read_write_ids), ite.read_write_mutex};
				if(ite_mutex.IsConflict(mutex))
				{
					auto num_order = property.priority <=> ite.priority;
					if(num_order == std::strong_ordering::greater)
					{
						auto re = AddDirectEdge_AssumedLocked(node, *preprocess_nodes[index].node);
						if(!re)
						{
							read_write_ids.resize(old_size);
							Remove_AssumedLocked(node);
							return false;
						}
					}else if(num_order == std::strong_ordering::less)
					{
						auto re = AddDirectEdge_AssumedLocked(*preprocess_nodes[index].node, node);
						if(!re)
						{
							read_write_ids.resize(old_size);
							Remove_AssumedLocked(node);
							return false;
						}
					}else
					{
						Order o1 = Order::UNDEFINE;
						Order o2 = Order::UNDEFINE;
						if(property.order_function != nullptr)
						{
							o1 = (*property.order_function)(disname, ite.name);
						}
						if(ite.order_func != nullptr && ite.order_func != property.order_function)
						{
							o2 = (*ite.order_func)(ite.name, disname);
						}
						if(
							o1 == Order::BIGGER && o2 == Order::SMALLER 
							|| o1 == Order::SMALLER && o2 == Order::BIGGER 
							|| o1 == Order::UNDEFINE && o2 == Order::UNDEFINE
							)
						{
							read_write_ids.resize(old_size);
							Remove_AssumedLocked(node);
							return false;
						}else if(o1 == Order::UNDEFINE)
						{
							o1 = o2;
						}
						if(o1 == Order::SMALLER)
						{
							auto re = AddDirectEdge_AssumedLocked(*preprocess_nodes[index].node, node);
							if(!re)
							{
								read_write_ids.resize(old_size);
								Remove_AssumedLocked(node);
								return false;
							}
						}else if(o1 == Order::BIGGER)
						{
							auto re = AddDirectEdge_AssumedLocked(node, *preprocess_nodes[index].node);
							if(!re)
							{
								read_write_ids.resize(old_size);
								Remove_AssumedLocked(node);
								return false;
							}
						}else if(o1 == Order::MUTEX)
						{
							auto re = AddMutexEdge_AssumedLocked(node, *preprocess_nodes[index].node);
							if(!re)
							{
								read_write_ids.resize(old_size);
								Remove_AssumedLocked(node);
								return false;
							}
						}
					}
				}
				++index;
			}
			mutex.index.components_span.WholeOffset(old_size);
			mutex.index.singleton_span.WholeOffset(old_size);
			mutex.index.user_modify.WholeOffset(old_size);
			system_mutex.emplace_back(
				mutex.index,
				property.priority,
				property.order_function,
				disname
			);
			return true;
		}
		return false;
	}
	void Context::Quit()
	{
		std::lock_guard lg(mutex);
		require_quit = true;
	}

	Context::Context(Config config, SyncResource resource)
		: config(config), 
		manager({
			resource.context_resource,
			resource.archetype_resource,
			resource.component_resource,
			resource.singleton_resource,
			resource.filter_resource,
			resource.temporary_resource
		}),
		system_resource(resource.system_resource),
		entity_resource(resource.entity_resource),
		temporary_resource(resource.temporary_resource),
		context_resource(resource.context_resource)
	{

	}

	bool Context::AddTickedSystemNode(SystemNode& node, SystemNodeProperty property)
	{
		ReadWriteMutexGenerator Generator(temporary_resource);
		node.FlushMutexGenerator(Generator);
		auto mutex = Generator.GetMutex();

		std::lock_guard lg(preprocess_mutex);
		for(auto& ite : preprocess_nodes)
		{
			auto ptr = static_cast<SubContextTaskFlow*>(ite.node.GetPointer());
			assert(ptr != nullptr);
			if(ptr->layout == property.priority.layout)
			{
				if(ptr->AddTickedNode(node, property))
				{
					return true;
				}else
				{
					return false;
				}
			}
		}
		auto re = Potato::IR::MemoryResourceRecord::Allocate<SubContextTaskFlow>(context_resource);
		if(re)
		{
			auto ptr = new(re.Get()) SubContextTaskFlow{re, property.priority.layout};
			assert(ptr);
			if(ptr->AddTickedNode(node, property))
			{
				AddNode_AssumedLocked(*ptr, {u8"sub_task"});
				for(auto& ite : preprocess_nodes)
				{
					auto tar = static_cast<SubContextTaskFlow*>(ite.node.GetPointer());
					if(tar != ptr)
					{
						if (tar->layout > property.priority.layout)
						{
							AddDirectEdge_AssumedLocked(*tar, *ptr);
						}
						else
						{
							assert(tar->layout != property.priority.layout);
							AddDirectEdge_AssumedLocked(*ptr, *tar);
						}
					}
				}
				return true;
			}
		}
		return false;
	}

	/*
	bool Context::AddSystem(SystemNode::Ptr system, SystemNodeProperty property)
	{
		if(system)
		{
			ReadWriteMutexGenerator Generator(temporary_resource);
			system->FlushMutexGenerator(Generator);
			auto mutex = Generator.GetMutex();

			std::lock_guard lg(preprocess_mutex);
			SubContextTaskFlow::Ptr target_flow;
			for(auto& ite : preprocess_nodes)
			{
				auto ptr = static_cast<SubContextTaskFlow*>(ite.node.GetPointer());
				assert(ptr != nullptr);
				if(ptr->layout == property.priority.layout)
				{
					target_flow = ptr;
					break;
				}
			}
			if(!target_flow)
			{
				auto re = Potato::IR::MemoryResourceRecord::Allocate<SubContextTaskFlow>(context_resource);
				if(re)
				{
					target_flow = new(re.Get()) SubContextTaskFlow{re, property.priority.layout};




					if(target_flow->AddNode(system.GetPointer(), {}))

					auto tem_node = target_flow->AddNode(
							system.GetPointer(), 
					{user_data->display_name, user_data->property.filter}, 
					user_data.GetPointer()
					);
					if(tem_node)
					{
						auto socket = AddNode_AssumedLock(target_flow.GetPointer());
						if(socket)
						{
							for(auto& ite : preprocess_nodes)
							{
								if(ite.node != target_flow)
								{
									auto ptr = static_cast<SubContextTaskFlow*>(ite.node.GetPointer());
									if(ptr->layout > property.priority.layout)
									{
										auto re = AddDirectEdge_AssumedLock(*ite.socket, *socket);
										assert(re);
									}else
									{
										auto re = AddDirectEdge_AssumedLock(*socket, *ite.socket);
										assert(re);
									}
								}
							}
							return true;
						}
					}
				}
			}
			else
			{
				std::lock_guard lg(target_flow->preprocess_mutex);
				auto tar = target_flow->AddNode_AssumedLock(system.GetPointer(), {user_data->display_name, user_data->property.filter}, user_data.GetPointer());
				if(tar)
				{
					for(auto& ite : target_flow->preprocess_nodes)
					{
						if(ite.socket != tar)
						{
							auto ud = static_cast<SystemNodeUserData*>(ite.socket->GetUserData().GetPointer());
							if(user_data->mutex.IsConflict(ud->mutex))
							{
								auto num_order = user_data->property.priority <=> ud->property.priority;
								if(num_order == std::strong_ordering::greater)
								{
									auto re = target_flow->AddDirectEdge_AssumedLock(*tar, *ite.socket);
									if(!re)
									{
										target_flow->Remove(*tar);
										return false;
									}
								}else if(num_order == std::strong_ordering::less)
								{
									auto re = target_flow->AddDirectEdge_AssumedLock(*ite.socket, *tar);
									if(!re)
									{
										target_flow->Remove(*tar);
										return false;
									}
								}else
								{
									Order o1 = Order::UNDEFINE;
									Order o2 = Order::UNDEFINE;
									if(user_data->property.order_function != nullptr)
									{
										o1 = (*user_data->property.order_function)(user_data->property.property, ud->property.property);
									}
									if(ud->property.order_function != nullptr && ud->property.order_function != user_data->property.order_function)
									{
										o2 = (*ud->property.order_function)(user_data->property.property, ud->property.property);
									}
									if(
										o1 == Order::BIGGER && o2 == Order::SMALLER 
										|| o1 == Order::SMALLER && o2 == Order::BIGGER 
										|| o1 == Order::UNDEFINE && o2 == Order::UNDEFINE
										)
									{
										target_flow->Remove(*tar);
										return false;
									}else if(o1 == Order::UNDEFINE)
									{
										o1 = o2;
									}
									if(o1 == Order::SMALLER)
									{
										auto re = target_flow->AddDirectEdge_AssumedLock(*ite.socket, *tar);
										if(!re)
										{
											target_flow->Remove(*tar);
											return false;
										}
									}else if(o1 == Order::BIGGER)
									{
										auto re = target_flow->AddDirectEdge_AssumedLock(*tar, *ite.socket);
										if(!re)
										{
											target_flow->Remove(*tar);
											return false;
										}
									}else if(o1 == Order::MUTEX)
									{
										auto re = target_flow->AddMutexEdge_AssumedLock(*tar, *ite.socket);
										if(!re)
										{
											target_flow->Remove(*tar);
											return false;
										}
									}
								}
							}
						}
					}
					return true;
				}
			}

			auto user_data = SystemNodeUserData::Create(property, mutex, system_resource);
			if(user_data)
			{
				
				
				
			}
		}
		return false;
	}
	*/

	void Context::TaskFlowExecuteBegin(Potato::Task::TaskFlowContext& context)
	{
		std::lock_guard lg(mutex);
		start_up_tick_lock = std::chrono::steady_clock::now();
	}

	void Context::TaskFlowExecuteEnd(Potato::Task::TaskFlowContext& context)
	{
		std::chrono::steady_clock::time_point now_time = std::chrono::steady_clock::now();
		std::chrono::steady_clock::time_point require_time;
		{
			std::lock_guard lg(mutex);
			if (require_quit)
			{
				require_quit = false;
				return;
			}
			require_time = start_up_tick_lock + config.min_frame_time;
		}

		manager.ForceUpdateState();
		Update();

		TaskFlow::Commited(context.context, require_time, context.node_property);
	}

	bool Context::Commited(Potato::Task::TaskContext& context, Potato::Task::TaskFlowNodeProperty property)
	{
		std::lock_guard lg(TaskFlow::process_mutex);
		if(current_status == Status::DONE || current_status == Status::READY)
		{
			manager.ForceUpdateState();
			Update_AssumedLocked();
			return TaskFlow::Commited_AssumedLocked(context, std::move(property));
		}
		return false;
	}
}