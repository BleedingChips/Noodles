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

	template<typename ...ComponentT>
	struct ComponentFilter
	{
		static ComponentFilter GenerateFilter(FilterGenerator& Generator)
		{
			static std::array<SystemRWInfo, sizeof...(ComponentT)> temp_buffer = {
				SystemRWInfo::Create<ComponentT>()...
			};

			return { Generator.CreateComponentFilter(std::span(temp_buffer)) };
		}
		ComponentFilter(ComponentFilter const&) = default;
		ComponentFilter(ComponentFilter&&) = default;
		ComponentFilter() = default;
	protected:
		ComponentFilter(SystemComponentFilter::Ptr filter) : filter(std::move(filter)) {}
		SystemComponentFilter::Ptr filter;

		friend struct Context;
	};


	template<typename ...ComponentT>
	struct EntityFilter
	{
		static EntityFilter GenerateFilter(FilterGenerator& Generator)
		{
			static std::array<SystemRWInfo, sizeof...(ComponentT)> temp_buffer = {
				SystemRWInfo::Create<ComponentT>()...
			};

			return { Generator.CreateEntityFilter(std::span(temp_buffer)) };
		}
		EntityFilter(EntityFilter const&) = default;
		EntityFilter(EntityFilter&&) = default;
		EntityFilter() = default;
	protected:
		EntityFilter(SystemEntityFilter::Ptr filter) : filter(std::move(filter)) {}
		SystemEntityFilter::Ptr filter;

		friend struct Context;
	};

	struct ContextConfig
	{
		std::chrono::milliseconds min_frame_time = std::chrono::milliseconds{ 13 };
	};

	using Priority = Potato::Task::Priority;
	using Category = Potato::Task::Category;

	struct ContextProperty
	{
		Priority priority = Priority::Normal;
		Category category = Category::GLOBAL_TASK;
		std::size_t group_id = 0;
		std::thread::id thread_id;
	};

	struct Context : public Potato::Task::Task, public Potato::Pointer::DefaultIntrusiveInterface
	{

		using Ptr = Potato::Pointer::IntrusivePtr<Context>;

		static Ptr Create(ContextConfig config, std::u8string_view context_name = u8"Noodles", std::pmr::memory_resource* UpstreamResource = std::pmr::get_default_resource());

		bool StartLoop(Potato::Task::TaskContext& context, ContextProperty property);

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

		template<typename Func>
		SystemRegisterResult RegisterTickSystemAutoDefer(
			std::int32_t layer, SystemPriority priority, SystemProperty property,
			Func&& func
		)
		{
			std::pmr::monotonic_buffer_resource temp;
			return tick_system_group.RegisterAutoDefer(
				component_manager, layer, priority, property, std::forward<Func>(func), context_name, &temp
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

		template<typename ...ParT, typename Func>
		bool Foreach(ComponentFilter<ParT...> const& filter, Func&& func) requires(std::is_invocable_r_v<bool, Func, std::span<ParT>...>)
		{
			if(filter.filter)
			{
				return filter.filter->Foreach(component_manager, [&](SystemComponentFilter::Wrapper wra) -> bool
					{
						std::tuple<std::span<ParT>...> temp_pars;
						SystemAutomatic::ApplySingleFilter<sizeof...(ParT)>::Apply(wra, temp_pars);
						return std::apply(func, temp_pars);
					});
			}
			return false;
		}


		template<typename Func>
		bool ForeachEntity(SystemEntityFilter const& filter, Entity const& entity, Func&& func) requires(std::is_invocable_r_v<bool, Func, SystemEntityFilter::Wrapper>)
		{
			return filter.ForeachEntity(component_manager, entity, std::forward<Func>(func));
		}

		template<typename ...ParT, typename Func>
		bool ForeachEntity(EntityFilter<ParT...> const& filter, Entity const& entity, Func&& func) requires(std::is_invocable_r_v<bool, Func, EntityStatus, std::span<ParT>...>)
		{
			if (filter.filter)
			{
				return component_manager.ReadEntityMountPoint(entity, [&](EntityStatus status, Archetype const& ar, ArchetypeMountPoint mp) -> bool
				{
					std::tuple<std::span<ParT>...> temp_pars;
					SystemAutomatic::ApplySingleFilter<sizeof...(ParT)>::Apply(ar, mp, temp_pars);
					return std::apply([&](auto& ...ar)
					{
						return func(status, std::forward<decltype(ar)&&>(ar)...);
					}, temp_pars);
				});
			}
			return false;
		}

	protected:

		virtual void operator()(Potato::Task::ExecuteStatus& Status) override;

		Context(ContextConfig config, std::u8string_view context_name, std::pmr::memory_resource* up_stream);

		//virtual void ControlRelease() override;
		virtual void AddRef() const override { return DefaultIntrusiveInterface::AddRef(); }
		virtual void SubRef() const override { return DefaultIntrusiveInterface::SubRef(); }
		virtual void Release() override;
		virtual ~Context() override = default;

		std::atomic_bool request_exit = false;

		std::atomic_size_t running_task;
		std::pmr::u8string context_name;

		std::mutex property_mutex;
		ContextConfig config;
		std::chrono::steady_clock::time_point last_execute_time;

		ArchetypeComponentManager component_manager;
		TickSystemsGroup tick_system_group;
		std::pmr::memory_resource* resource;

		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
		
	};

	
}
