module;

#include <cassert>

module NoodlesContext;

namespace Noodles
{


	constexpr std::u8string_view TaskName = u8"Noodles Startup";

	auto Context::Create(ContextConfig config, Potato::Task::TaskContext::Ptr TaskPtr, std::u8string_view context_name, std::pmr::memory_resource* UpstreamResource)->Ptr
	{
		if(TaskPtr && UpstreamResource != nullptr)
		{
			auto Adress = UpstreamResource->allocate(sizeof(Context), alignof(Context));
			if(Adress != nullptr)
			{
				return new (Adress) Context{ config, std::move(TaskPtr), context_name, UpstreamResource };
			}else
			{
				return {};
			}
		}
		return {};
	}

	Context::Context(ContextConfig config, Potato::Task::TaskContext::Ptr TaskPtr, std::u8string_view context_name, std::pmr::memory_resource* up_stream)
		: task_context(std::move(TaskPtr)), context_name(up_stream), resource(up_stream), config(config), component_manager(up_stream), tick_system_group(up_stream)
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
		if(status.Property.AppendData == 0)
		{
			{
				std::lock_guard lg2(property_mutex);
				last_execute_time = std::chrono::system_clock::now();
			}

			component_manager.UpdateEntityStatus();
			auto has_system = tick_system_group.SynFlushAndDispatch(component_manager, [&](TickSystemRunningIndex ruing_index, std::u8string_view display_name)
			{
				auto new_property = status.Property;
				new_property.AppendData = ruing_index.index + 1;
				new_property.AppendData2 = ruing_index.parameter;
				new_property.TaskName = display_name;
				status.Context.CommitTask(
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
			auto index = status.Property.AppendData - 1;

			auto re = tick_system_group.ExecuteAndDispatchDependence(
				{index, status.Property.AppendData2},
				*this,
				[&](TickSystemRunningIndex i, std::u8string_view dis)
				{
					auto new_property = status.Property;
					new_property.AppendData = i.index + 1;
					new_property.AppendData2 = i.parameter;
					new_property.TaskName = dis;
					status.Context.CommitTask(
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
				std::chrono::system_clock::time_point require_time;
				Potato::Task::TaskContext::Ptr re_task_context;
				auto new_pro = status.Property;

				new_pro.AppendData = 0;
				std::lock_guard lg(property_mutex);
				require_time = last_execute_time + config.min_frame_time;
				re_task_context = task_context;
				new_pro.TaskPriority = config.priority;
				assert(re_task_context);
				re_task_context->CommitDelayTask(this, require_time, new_pro);
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

	bool Context::StartLoop()
	{
		if(!tick_system_group.Empty())
		{
			std::lock_guard lg(property_mutex);
			std::size_t E = 0;
			if (task_context && running_task.compare_exchange_strong(E, 2))
			{

				if (!task_context->CommitTask(this, {
					config.priority,
					TaskName,
					0, 0
					}))
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