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
		volatile int i = 0;
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
		return TaskFlow::Commit(context, property);
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
		TaskFlow::CommitDelay(status.context, *start_up_tick_lock + config.min_frame_time, status.task_property);
	}

	bool Context::RegisterSystem(SystemHolder::Ptr ptr, std::int32_t layer, Priority priority, Property property, OrderFunction func, Potato::Task::TaskProperty task_property, ReadWriteMutexGenerator& generator)
	{
		std::lock_guard lg(system_mutex);
		if(ptr)
		{
			auto tptr = Potato::Task::TaskFlowNode::Ptr{ptr.GetPointer()};

			if(AddNode(tptr, task_property))
			{
				for (auto& ite : systems)
				{

				}

				systems.emplace_back(
					ptr,
					property,
					priority,
					Potato::Misc::IndexSpan<>{},
					Potato::Misc::IndexSpan<>{},
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