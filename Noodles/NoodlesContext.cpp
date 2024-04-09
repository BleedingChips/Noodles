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
			auto re = ite1->type_id <=> ite2->type_id;
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
		return DetectConflict(components, mutex.components) || DetectConflict(singleton, mutex.singleton);
	}

	void ReadWriteMutexGenerator::RegisterComponentMutex(std::span<RWUniqueTypeID const> ifs)
	{
		for(auto& ite : ifs)
		{
			auto spn = std::span(unique_ids).subspan(0, component_count);
			bool Find = false;
			for(auto& ite2 : spn)
			{
				if(ite.type_id == ite2.type_id)
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
			auto spn = std::span(unique_ids).subspan(component_count);
			bool Find = false;
			for (auto& ite2 : spn)
			{
				if (ite.type_id == ite2.type_id)
				{
					ite2.is_write = ite2.is_write || ite.is_write;
					Find = true;
					break;
				}
			}
			if (!Find)
			{
				unique_ids.emplace_back(ite);
			}
		}
	}

	ReadWriteMutex ReadWriteMutexGenerator::GetMutex() const
	{
		return ReadWriteMutex{
			std::span(unique_ids).subspan(0, component_count),
			std::span(unique_ids).subspan(component_count),
			system_id
		};
	}

	static Potato::Format::StaticFormatPattern<u8"{}{}{}-[{}]:[{}]"> system_static_format_pattern;

	std::size_t SystemHolder::FormatDisplayNameSize(std::u8string_view prefix, Property property)
	{
		Potato::Format::FormatWritter<char8_t> wri;
		system_static_format_pattern.Format(wri, property.group, property.name, prefix, property.group, property.name);
		return wri.GetWritedSize();
	}

	std::optional<std::tuple<std::u8string_view, Property>> SystemHolder::FormatDisplayName(std::span<char8_t> output, std::u8string_view prefix, Property property)
	{
		Potato::Format::FormatWritter<char8_t> wri(output);
		auto re = system_static_format_pattern.Format(wri, property.group, property.name, prefix, property.group, property.name);
		if(re)
		{
			return std::tuple<std::u8string_view, Property>{
				std::u8string_view{output.data(), wri.GetWritedSize()}.substr(property.group.size() + property.name.size()),
				Property{
					std::u8string_view{output.data() + property.group.size(), property.name.size()},
					std::u8string_view{output.data(), property.group.size()},
				}
			};
		}
		return std::nullopt;
	}

	void SystemHolder::TaskFlowNodeExecute(Potato::Task::TaskFlowStatus& status)
	{
		ExecuteContext context
		{
			status.context,
			static_cast<Context&>(status.owner)
		};
		SystemExecute(context);
	}

	void Context::AddTaskRef() const
	{
		DefaultIntrusiveInterface::AddRef();
	}

	void Context::SubTaskRef() const
	{
		DefaultIntrusiveInterface::SubRef();
	}

	void Context::Release()
	{
		auto re = record;
		this->~Context();
		re.Deallocate();
	}

	auto Context::Create(Config config, std::u8string_view name, std::pmr::memory_resource* resource) -> Ptr
	{
		auto fix_layout = Potato::IR::Layout::Get<Context>();
		std::size_t offset = 0;
		if(!name.empty())
		{
			offset = Potato::IR::InsertLayoutCPP(fix_layout, Potato::IR::Layout::GetArray<char8_t>(name.size()));
			Potato::IR::FixLayoutCPP(fix_layout);
		}

		auto re = Potato::IR::MemoryResourceRecord::Allocate(resource, fix_layout);
		if(re)
		{
			if(!name.empty())
			{
				std::memcpy(re.GetByte() + offset, name.data(), sizeof(char8_t) * name.size());
				name = std::u8string_view{ reinterpret_cast<char8_t const*>(re.GetByte() + offset), name.size() };
			}
			
			return new (re.Get()) Context{config, name, re};
		}
		return {};
	}

	bool Context::Commit(Potato::Task::TaskContext& context, Potato::Task::TaskProperty property)
	{
		TaskFlow::Update(true);
		return TaskFlow::Commit(context, property, name);
	}

	void Context::OnBeginTaskFlow(Potato::Task::ExecuteStatus& status)
	{
		std::lock_guard lg(mutex);
		if(!start_up_tick_lock.has_value())
		{
			start_up_tick_lock = std::chrono::steady_clock::now();
			manager.ForceUpdateState();
		}else
		{
			start_up_tick_lock = std::chrono::steady_clock::now();
		}
		std::println("---start");
	}

	void Context::OnFinishTaskFlow(Potato::Task::ExecuteStatus& status)
	{
		TaskFlow::Update(true);
		manager.ForceUpdateState();
		
		std::lock_guard lg(mutex);
		std::println("---finish");
		if (require_quit)
		{
			require_quit = false;
			return;
		}
		TaskFlow::CommitDelay(status.context, *start_up_tick_lock + config.min_frame_time, status.task_property, name);
	}

	bool Context::RegisterSystem(SystemHolder::Ptr ptr, Priority priority, Property property, OrderFunction func, Potato::Task::TaskProperty task_property, ReadWriteMutexGenerator& generator)
	{
		std::lock_guard lg(system_mutex);
		if(ptr)
		{

			Potato::Task::TaskFlowNode::Ptr tptr = ptr.GetPointer();

			if(AddNode(tptr, task_property, ptr->GetDisplayName()))
			{
				auto mutex = generator.GetMutex();

				for(auto& ite : systems)
				{
					auto re = ite.priority <=> priority;
					bool is_conflict = false;

					if (ite.priority.layout != priority.layout)
					{
						is_conflict = true;
					}
					else 
					{
						ReadWriteMutex ite_mutex{
						ite.component_index.Slice(std::span(rw_unique_id)),
						ite.singleton_index.Slice(std::span(rw_unique_id)),
							generator.system_id
						};
						is_conflict = ite_mutex.IsConflict(mutex);
						if(is_conflict && re == std::strong_ordering::equal)
						{
							auto p1 = (ite.order_function == nullptr) ?
								std::partial_ordering::unordered : ite.order_function(ite.property, property);
							auto p2 = (func == nullptr) ?
								std::partial_ordering::unordered : func(ite.property, property);
							auto p3 = std::partial_ordering::unordered;
							if (p1 == p2)
								p3 = p1;
							else if (p1 == std::partial_ordering::unordered || p1 == std::partial_ordering::equivalent)
								p3 = p2;
							else if (p2 == std::partial_ordering::unordered || p2 == std::partial_ordering::equivalent)
								p3 = p1;
							else
							{
								Remove(tptr);
								return false;
							}
							if (p3 == std::partial_ordering::less)
								re = std::strong_ordering::less;
							else if(p3 == std::partial_ordering::greater)
								re = std::strong_ordering::greater;
						}
					}

					if(is_conflict)
					{
						Potato::Task::TaskFlowNode::Ptr tptr2{ite.system.GetPointer()};
						if (re == std::strong_ordering::less)
							AddDirectEdges(std::move(tptr2), tptr);
						else if (re == std::strong_ordering::greater)
							AddDirectEdges(tptr, std::move(tptr2));
						else
							AddMutexEdges(tptr, std::move(tptr2));
					}
				}

				auto osize = rw_unique_id.size();
				rw_unique_id.insert(rw_unique_id.end(), mutex.components.begin(), mutex.components.end());
				Potato::Misc::IndexSpan<> comp_ind{osize, rw_unique_id.size()};
				osize = rw_unique_id.size();
				rw_unique_id.insert(rw_unique_id.end(), mutex.singleton.begin(), mutex.singleton.end());
				Potato::Misc::IndexSpan<> single_ind{ osize, rw_unique_id.size() };

				systems.emplace_back(
					ptr,
					property,
					priority,
					comp_ind,
					single_ind,
					mutex.system,
					func
				);

				return true;
			}


			
		}
		return false;
	}

	/*
	constexpr std::u8string_view TaskName = u8"Noodles Startup";

	auto Context::Create(ContextConfig config, std::u8string_view context_name, std::pmr::memory_resource* UpstreamResource)->Ptr
	{
		if(UpstreamResource != nullptr)
		{
			auto Adress = UpstreamResource->allocate(sizeof(Context), alignof(Context));
			if(Adress != nullptr)
			{
				return new (Adress) Context{ config, context_name, UpstreamResource };
			}else
			{
				return {};
			}
		}
		return {};
	}

	Context::Context(ContextConfig config, std::u8string_view context_name, std::pmr::memory_resource* up_stream)
		: context_name(up_stream), resource(up_stream), config(config), component_manager(up_stream), tick_system_group(up_stream)
	{
		this->context_name = context_name;
	}

	bool Context::RequireExist()
	{
		bool re = false;
		return request_exit.compare_exchange_strong(re, true);
	}

	void Context::operator()(Potato::Task::ExecuteStatus& status)
	{
		if(status.task_property.user_data[0] == 0)
		{
			{
				std::lock_guard lg2(property_mutex);
				last_execute_time = std::chrono::steady_clock::now();
			}

			component_manager.UpdateEntityStatus();
			auto has_system = tick_system_group.SynFlushAndDispatch(component_manager, [&](TickSystemRunningIndex ruing_index, std::u8string_view display_name)
			{
				auto new_property = status.task_property;
				new_property.user_data[0] = ruing_index.index + 1;
				new_property.user_data[1] = ruing_index.parameter;
				new_property.name = display_name;
				status.context.CommitTask(
					this,
					new_property
				);
				std::size_t E = 0;
				while (!running_task.compare_exchange_strong(
					E, E + 1
				))
				{

				}
			});
			if(!has_system)
			{
				request_exit = true;
			}
		}else
		{
			auto index = status.task_property.user_data[0] - 1;

			auto re = tick_system_group.ExecuteAndDispatchDependence(
				{index, status.task_property.user_data[1]},
				*this,
				[&](TickSystemRunningIndex i, std::u8string_view dis)
				{
					auto new_property = status.task_property;
					new_property.user_data[0] = i.index + 1;
					new_property.user_data[1] = i.parameter;
					new_property.name = dis;
					status.context.CommitTask(
						this,
						new_property
					);
					std::size_t E = 0;
					while (!running_task.compare_exchange_strong(
						E, E + 1
					))
					{

					}
				}
			);
		}

		std::size_t E = 0;
		while (!running_task.compare_exchange_strong(
			E, E - 1
		))
		{

		}

		if (E == 2)
		{
			if(!request_exit)
			{
				E = 1;
				auto re = running_task.compare_exchange_strong(E, 2);
				assert(re);
				std::chrono::steady_clock::time_point require_time;
				auto new_pro = status.task_property;

				new_pro.user_data[0] = 0;
				new_pro.name = context_name;

				{
					std::lock_guard lg(property_mutex);
					require_time = last_execute_time + config.min_frame_time;
					status.context.CommitDelayTask(this, require_time, new_pro);
				}
			}
		}
	}

	void Context::Release()
	{
		auto o_resource = resource;
		this->~Context();
		o_resource->deallocate(
			this,
			sizeof(Context),
			alignof(Context)
		);
	}

	bool Context::StartLoop(Potato::Task::TaskContext& context, ContextProperty property)
	{
		if(!tick_system_group.Empty())
		{
			std::lock_guard lg(property_mutex);
			std::size_t E = 0;
			if (running_task.compare_exchange_strong(E, 2))
			{
				Potato::Task::TaskProperty tp{
					property.priority,
					context_name,
					{0, 0},
					property.category,
					property.group_id,
					property.thread_id
				};

				if (!context.CommitTask(this, tp))
				{
					E = 2;
					auto re = running_task.compare_exchange_strong(E, 0);
					assert(re);
					return false;
				}
				return true;
			}
		}
		return false;
	}

	bool Context::StartSelfParallel(SystemContext& context, std::size_t count)
	{
		return tick_system_group.StartParallel(context.ptr, count);
	}
	*/
}