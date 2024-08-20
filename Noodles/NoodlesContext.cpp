module;

#include <cassert>

module NoodlesContext;
import PotatoFormat;

namespace Noodles
{

	bool DetectConflict(std::span<RWUniqueTypeID const> t1, std::span<RWUniqueTypeID const> t2)
	{
		for(auto& ite : t1)
		{
			assert(!ite.ignore_mutex);
			for(auto& ite2 : t2)
			{
				if(ite.is_write || ite2.is_write)
				{
					if(*ite.atomic_type == *ite2.atomic_type)
					{
						return false;
					}
				}
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
				unique_ids.insert(unique_ids.begin() + component_count + singleton_count, ite);
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
		SubContextTaskFlow* flow = static_cast<SubContextTaskFlow*>(status.flow.GetPointer());
		Context* Tar = nullptr;

		{
			std::lock_guard lg(flow->process_mutex);
			Tar = static_cast<Context*>(flow->parent_node.GetPointer());
		}

		ExecuteContext exe_context
		{
			*Tar,
			*flow,
			status.node_property.display_name
		};

		SystemNodeExecute(exe_context);
	}

	static Potato::Format::StaticFormatPattern<u8"{}:{}"> system_static_format_pattern;

	Potato::IR::Layout SystemName::GetSerializeLayout() const
	{
		Potato::Format::FormatWritter<char8_t> temp_writer;
		auto re = system_static_format_pattern.Format(temp_writer, group, name);
		assert(re);
		return Potato::IR::Layout::GetArray<char8_t>(temp_writer.GetWritedSize());
	}

	void SystemName::SerializeTo(std::span<std::byte> output) const
	{
		Potato::Format::FormatWritter<char8_t> temp_writer{
			std::span<char8_t>{reinterpret_cast<char8_t*>(output.data()), output.size()}
		};
		auto re = system_static_format_pattern.Format(temp_writer, group, name);
		assert(re);
	}
	SystemName SystemName::ReMap(std::span<std::byte> input) const
	{
		return {
			std::u8string_view{reinterpret_cast<char8_t*>(input.data() + sizeof(char8_t) * ( group.size() + 1)), name.size()},
			std::u8string_view{reinterpret_cast<char8_t*>(input.data()), group.size()}
		};
	}

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
			std::size_t old_size = preprocess_rw_id.size();
			preprocess_rw_id.insert(preprocess_rw_id.end(), 
				std::move_iterator(gener.unique_ids.begin()), 
				std::move_iterator(gener.unique_ids.end())
			);
			mutex.total_type_id = std::span(preprocess_rw_id).subspan(old_size);
			std::size_t index = 0;
			auto disname = node.GetDisplayName();
			for(auto& ite : preprocess_system_infos)
			{
				ReadWriteMutex ite_mutex{std::span(preprocess_rw_id), ite.read_write_mutex};
				if(ite_mutex.IsConflict(mutex))
				{
					auto num_order = property.priority <=> ite.priority;
					if(num_order == std::strong_ordering::greater)
					{
						auto re = AddDirectEdge_AssumedLocked(node, *preprocess_nodes[index].node);
						if(!re)
						{
							preprocess_rw_id.resize(old_size);
							Remove_AssumedLocked(node);
							return false;
						}
					}else if(num_order == std::strong_ordering::less)
					{
						auto re = AddDirectEdge_AssumedLocked(*preprocess_nodes[index].node, node);
						if(!re)
						{
							preprocess_rw_id.resize(old_size);
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
							preprocess_rw_id.resize(old_size);
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
								preprocess_rw_id.resize(old_size);
								Remove_AssumedLocked(node);
								return false;
							}
						}else if(o1 == Order::BIGGER)
						{
							auto re = AddDirectEdge_AssumedLocked(node, *preprocess_nodes[index].node);
							if(!re)
							{
								preprocess_rw_id.resize(old_size);
								Remove_AssumedLocked(node);
								return false;
							}
						}else if(o1 == Order::MUTEX)
						{
							auto re = AddMutexEdge_AssumedLocked(node, *preprocess_nodes[index].node);
							if(!re)
							{
								preprocess_rw_id.resize(old_size);
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
			preprocess_system_infos.emplace_back(
				mutex.index,
				property.priority,
				property.order_function,
				disname
			);
			return true;
		}
		return false;
	}

	bool SubContextTaskFlow::AddTemporaryNode(SystemNode& node, Potato::Task::TaskFilter filter, std::pmr::memory_resource* temp_resource)
	{
		ReadWriteMutexGenerator ge(temp_resource);
		auto s_node = static_cast<SystemNode const*>(&node);
		s_node->FlushMutexGenerator(ge);
		auto rw_mutex = ge.GetMutex();
		auto name = node.GetDisplayName();
		std::lock_guard lg(process_mutex);
		if(TaskFlow::AddTemporaryNode_AssumedLocked(
			node,
			{name.name, filter},
			[=, this](TaskFlowNode const& node, Potato::Task::TaskFlowNodeProperty property, TemporaryNodeIndex index)-> bool
			{
				assert(index.current_index < process_system_infos.size());
				ReadWriteMutex index_rw_mutex{
					std::span(process_rw_id),
					process_system_infos[index.current_index].read_write_mutex
				};
				return rw_mutex.IsConflict(index_rw_mutex);
			}
		))
		{
			auto old_size = process_rw_id.size();
			process_rw_id.insert(process_rw_id.end(), 
				std::move_iterator(ge.unique_ids.begin()), std::move_iterator(ge.unique_ids.end()));
			rw_mutex.index.components_span.WholeOffset(old_size);
			rw_mutex.index.singleton_span.WholeOffset(old_size);
			rw_mutex.index.user_modify.WholeOffset(old_size);
			process_system_infos.emplace_back(rw_mutex.index);
			return true;
		}
		return false;
	}

	bool SubContextTaskFlow::Update_AssumedLocked()
	{
		bool par = need_update;
		if(TaskFlow::Update_AssumedLocked())
		{
			if(par)
			{
				process_rw_id = preprocess_rw_id;
				process_system_infos.clear();
				for(auto& ite : preprocess_system_infos)
				{
					process_system_infos.emplace_back(ite.read_write_mutex);
				}
				temporary_rw_id_offset = process_rw_id.size();
			}else
			{
				process_system_infos.resize(temporary_node_offset);
				process_rw_id.resize(temporary_rw_id_offset);
			}
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