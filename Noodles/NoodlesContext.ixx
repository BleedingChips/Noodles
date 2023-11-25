module;

export module NoodlesContext;

import std;

import PotatoMisc;
import PotatoPointer;
export import PotatoTaskSystem;

import NoodlesMemory;
export import NoodlesSystem;



export namespace Noodles
{

	struct ContextConfig
	{
		std::size_t priority = *Potato::Task::TaskPriority::Normal;
		std::chrono::milliseconds min_frame_time = std::chrono::milliseconds{ 13 };
	};

	struct Context : public Potato::Task::Task
	{

		using Ptr = Potato::Pointer::IntrusivePtr<Context>;

		static Ptr Create(ContextConfig config, Potato::Task::TaskContext::Ptr ptr, std::u8string_view context_name = u8"Noodles", std::pmr::memory_resource* UpstreamResource = std::pmr::get_default_resource());

		bool StartLoop();

		template<typename GeneratorFunc, typename Func>
		SystemRegisterResult RegisterTickSystemDefer(
			std::int32_t layer, SystemPriority priority, SystemProperty property,
			GeneratorFunc&& g_func, Func&& func
		)
			requires(
		std::is_invocable_v<GeneratorFunc, FilterGenerator&>
			&& !std::is_same_v<decltype(g_func(std::declval<FilterGenerator&>())), void>&&
			std::is_invocable_v<Func, SystemContext&, std::remove_cvref_t<decltype(g_func(std::declval<FilterGenerator&>()))>&>
			)
		{
			std::pmr::monotonic_buffer_resource temp;
			return tick_system_group.RegisterDefer(
			component_manager, layer, priority, property, std::forward<GeneratorFunc>(g_func), std::forward<Func>(func), context_name, &temp
			);
		}

		bool RequireExist();

		bool StartSelfParallel(SystemContext& context,  std::size_t count);

		EntityPtr CreateEntityDefer(EntityConstructor const& constructor) { return component_manager.CreateEntityDefer(constructor); }


		template<typename Func>
		bool Foreach(SystemComponentFilter const& filter, Func&& func) requires(std::is_invocable_r_v<bool, Func, SystemComponentFilter::Wrapper>)
		{
			return filter.Foreach(component_manager, std::forward<Func>(func));
		}


		template<typename Func>
		bool ForeachEntity(SystemEntityFilter const& filter, Entity const& entity, Func&& func) requires(std::is_invocable_r_v<bool, Func, SystemEntityFilter::Wrapper>)
		{
			return filter.ForeachEntity(component_manager, entity, std::forward<Func>(func));
		}

	protected:

		virtual void operator()(Potato::Task::ExecuteStatus& Status) override;

		Context(ContextConfig config, Potato::Task::TaskContext::Ptr TaskPtr, std::u8string_view context_name, std::pmr::memory_resource* up_stream);

		//virtual void ControlRelease() override;
		virtual void Release() override;
		virtual ~Context() override = default;

		std::atomic_bool request_exit = false;

		std::atomic_size_t running_task;
		std::pmr::u8string context_name;

		std::mutex property_mutex;
		Potato::Task::TaskContext::Ptr task_context;
		ContextConfig config;
		std::chrono::system_clock::time_point last_execute_time;

		ArchetypeComponentManager component_manager;
		TickSystemsGroup tick_system_group;
		std::pmr::memory_resource* resource;
		
	};
}
