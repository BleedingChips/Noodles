module;

#include <cassert>

module NoodlesContext;

namespace Noodles
{


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
}