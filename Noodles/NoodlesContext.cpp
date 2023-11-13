module;

#include <cassert>

module NoodlesContext;

namespace Noodles
{

	/*
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

	void Context::operator()(Potato::Task::ExecuteStatus& status)
	{
		if(status.Property.AppendData == 0)
		{
			{
				std::lock_guard lg2(property_mutex);
				last_execute_time = std::chrono::system_clock::now();
			}

			component_manager.UpdateEntityStatus();
			auto count = tick_system_group.SynFlushAndDispatch(component_manager, [&](std::size_t index, std::u8string_view display_name)
			{
				auto new_property = status.Property;
				new_property.AppendData = index + 1;
				new_property.AppendData2 = 0;
				new_property.TaskName = display_name;
				task_context->CommitTask(
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
		}else
		{
			auto index = status.Property.AppendData - 1;
			if(status.Property.AppendData2 == 0)
			{
				tick_system_group.ExecuteSystem(index, component_manager, *this);
				status.Property.AppendData2 = 1;
			}

			auto re = tick_system_group.TryDispatchDependence(index, [&](std::size_t i, std::u8string_view dis)
			{
					auto new_property = status.Property;
					new_property.AppendData = i + 1;
					new_property.AppendData2 = 0;
					new_property.TaskName = dis;
					task_context->CommitTask(
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

			if(!re.has_value())
			{
				task_context->CommitTask(
					this,
					status.Property
				);
				return;
			}
		}

		std::size_t E = 0;
		while (!running_task.compare_exchange_strong(
			E, E - 1
		))
		{

		}

		if (E == 2)
		{
			E = 1;
			auto re = running_task.compare_exchange_strong(E, 2);
			assert(re);
			std::chrono::system_clock::time_point require_time;
			Potato::Task::TaskContext::Ptr re_task_context;
			auto new_pro = status.Property;

			new_pro.AppendData = 0;
			{
				std::lock_guard lg(property_mutex);
				require_time = last_execute_time + config.min_frame_time;
				re_task_context = task_context;
				new_pro.TaskPriority = config.priority;
			}
			assert(re_task_context);
			re_task_context->CommitDelayTask(this, require_time, new_pro);
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
		std::lock_guard lg(property_mutex);
		std::size_t E = 0;
		if(task_context && running_task.compare_exchange_strong(E, 2))
		{
			
			if(!task_context->CommitTask(this, {
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
		return false;
	}
	*/
}