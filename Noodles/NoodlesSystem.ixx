module;

export module NoodlesSystem;

import std;
import PotatoTMP;
import PotatoMisc;
import PotatoPointer;
import PotatoTaskSystem;
import PotatoIR;

import NoodlesArchetype;
export import NoodlesComponent;

export namespace Noodles
{
	struct Context;

	struct SystemRWInfo
	{

		bool is_write = false;
		UniqueTypeID type_id;

		template<typename Type>
		static SystemRWInfo GetComponent()
		{
			return SystemRWInfo{
				!std::is_const_v<Type>,
				UniqueTypeID::Create<std::remove_cvref_t<Type>>()
			};
		}

		std::strong_ordering operator<=>(SystemRWInfo const& i) const { return type_id <=> i.type_id; }

	};

	struct SystemProperty;

	struct SystemPriority
	{
		std::int32_t primary_priority = 0;
		std::int32_t second_priority = 0;
		std::partial_ordering (*compare)(SystemProperty const& self, SystemProperty const& target) = nullptr;

		std::strong_ordering ComparePriority(SystemPriority const& p2) const;
		std::partial_ordering CompareCustomPriority(SystemProperty const& self_property, SystemPriority const& target, SystemProperty const& target_property) const;
	};

	struct SystemProperty
	{
		std::u8string_view system_name;
		std::u8string_view group_name;
		
		bool IsSameSystem(SystemProperty const& oi) const
		{
			return group_name == oi.group_name && system_name == oi.system_name;
		}
	};

	struct SystemMutex
	{
		std::span<SystemRWInfo const> component_rw_infos;
		//std::span<RWInfo const> global_component_rw_infos;

		bool IsConflict(SystemMutex const& p2) const;
	};

	struct FilterCollection
	{
		FilterCollection(std::pmr::memory_resource* resource)
			: component_filters(resource), rw_infos(resource) {}
		SystemMutex GetMutex() const { return {std::span(rw_infos)}; }
	public:
		std::pmr::vector<ComponentFilterWrapper::CPtr> component_filters;
		std::pmr::vector<SystemRWInfo> rw_infos;
	};

	struct FilterGenerator
	{
		FilterGenerator(std::pmr::memory_resource* resource, ArchetypeComponentManager& manager)
			: collection(resource), manager(manager) { }

		std::size_t RegisterComponentFilter(std::span<SystemRWInfo const> ifs);

		FilterCollection collection;

	protected:

		ArchetypeComponentManager& manager;
		std::pmr::memory_resource* temp_memory_resource;
	};

	struct SystemContext
	{
		SystemProperty self_property;
		ArchetypeComponentManager& manager;
		FilterCollection& filter_collection;
		Context& global_context;
	};

	export enum class RunningStatus
	{
		PreInit,
		Waiting,

		Ready,
		Running,
		Done,
	};

	export struct SystemHolder : public Potato::Pointer::DefaultIntrusiveInterface
	{

		using Ptr = Potato::Pointer::IntrusivePtr<SystemHolder>;

		SystemProperty GetProperty() const {
			return {
				std::u8string_view{group_name },
				std::u8string_view{ system_name }
			};
		}

		std::u8string_view GetDisplayName() const { return std::u8string_view{ display_name }; }

		SystemMutex GetMutexProperty() const
		{
			return filter_collection.GetMutex();
		};

		template<typename Func>
		static auto Create(
			Func&& func,
			SystemPriority const& priority,
			SystemProperty const& property,
			FilterGenerator const& generator,
			std::pmr::memory_resource* resource
			)
		->Ptr;

		virtual void Execute(Context&) = 0;

	protected:

		SystemHolder(
			//Priority const& priority,
			SystemProperty const& property,
			FilterGenerator const& generator,
			std::pmr::memory_resource* resource
		);

		RunningStatus status = RunningStatus::PreInit;

		std::size_t startup_in_degree = 0;
		std::size_t current_in_degree = 0;
		std::size_t mutex_degree = 0;
		Potato::Misc::IndexSpan<> reference_trigger_line;

		std::pmr::u8string group_name;
		std::pmr::u8string system_name;
		std::pmr::u8string display_name;
		FilterCollection filter_collection;
	};

	export struct SystemTriggerLine
	{
		bool is_mutex = false;
		SystemHolder::Ptr target = nullptr;
	};

	struct SystemTemporaryDependenceLine
	{
		bool is_mutex;
		std::size_t from;
		std::size_t to;
	};

	export struct SystemRegisterResult
	{
		enum class Status
		{
			Available,
			EmptyName,
			ExistName,
			ConfuseDependence,
			CircleDependence,
		};

		Status status = Status::Available;
		std::size_t ite_index = 0;
		std::size_t in_degree = 0;
		operator bool() const { return status == Status::Available; }
	};

	struct TickSystemsGroup
	{

		TickSystemsGroup(std::pmr::memory_resource* resource = std::pmr::get_default_resource());

	protected:

		SystemRegisterResult PreLoginCheck(std::int32_t layer, SystemPriority priority, SystemProperty property, SystemMutex const& pro, std::pmr::vector<SystemTemporaryDependenceLine>& dep);
		void Login(SystemRegisterResult const& Result, std::pmr::vector<SystemTemporaryDependenceLine> const&, SystemHolder::Ptr TargetPtr);


		struct GraphicNode
		{
			struct TriggerTo
			{
				bool is_mutex;
				std::size_t to;
			};

			std::int32_t layer = 0;
			SystemPriority priority;
			SystemProperty property;
			SystemMutex mutex_property;
			std::pmr::vector<TriggerTo> trigger_to;
			std::size_t in_degree = 0;
			SystemHolder::Ptr system_obj;
		};

		std::pmr::synchronized_pool_resource system_holder_resource;

		std::mutex graphic_mutex;
		std::pmr::vector<GraphicNode> graphic_node;
		bool need_refresh_dependence = false;

		struct StartupSystem
		{
			std::int32_t layout = 0;
			SystemHolder::Ptr system_obj;
		};

		std::mutex tick_system_running_mutex;
		std::pmr::vector<StartupSystem> startup_system_context;
		std::size_t startup_system_context_ite = 0;
		std::pmr::vector<SystemTriggerLine> tick_systems_running_graphic_line;
		std::size_t current_level_system_waiting = 0;

	public:

		template<typename GeneratorFunc, typename Func>
		SystemRegisterResult RegisterDefer(ArchetypeComponentManager& manager, std::int32_t layer, SystemPriority priority, SystemProperty property,
			GeneratorFunc&& g_func, Func&& func, std::pmr::memory_resource* temporary_resource = std::pmr::get_default_resource())
			requires(
		std::is_invocable_v<GeneratorFunc, FilterGenerator&>&& std::is_invocable_v<Func, SystemContext&>
			)
		{
			std::lock_guard lg(graphic_mutex);
			std::pmr::monotonic_buffer_resource temp_resource(temporary_resource);
			FilterGenerator generator(&system_holder_resource, manager);
			g_func(generator);
			std::pmr::vector<SystemTemporaryDependenceLine> temp_line{&temp_resource };
			auto re = PreLoginCheck(layer, priority, property, generator.collection.GetMutex(), temp_line);
			if(re)
			{
				auto ptr = SystemHolder::Create(
					std::move(func), priority, property, generator, &system_holder_resource
				);
				Login(re, temp_line, std::move(ptr));
			}
			return re;
		}

		template<typename Func>
		std::size_t SynFlushAndDispatch(Func&& func) requires(std::is_invocable_v<Func, SystemHolder::Ptr&>);

		template<typename Func>
		std::optional<std::size_t> TryDispatchDependence(Func&& func) requires(std::is_invocable_v<Func, SystemHolder::Ptr&>);
	};


	template<typename Func>
	concept AcceptableSystemObject = true;

	template<typename ParT>
	struct IsAcceptableParameter
	{
		using PT = std::remove_cvref_t<ParT>;
		static constexpr bool value = std::is_same_v<SystemContext, PT> || IsAcceptableComponentFilterV<ParT>;
	};

	template<typename ...ParT>
	struct IsAcceptableParameters
	{
		static constexpr bool value = (true && ... && IsAcceptableParameter<ParT>::value);
	};

	export template<typename ToT>
		struct ExecuteContextDistributor
	{
		ToT operator()(SystemContext& context) = delete;
	};

	export template<>
		struct ExecuteContextDistributor<SystemContext>
	{
		SystemContext& operator()(SystemContext& context) { return context; }
	};

	export template<typename ...ToT>
		struct ExecuteContextDistributors
	{
		template<typename Func>
		void operator()(SystemContext& context, Func&& func)
		{
			func(ExecuteContextDistributor<std::remove_cvref_t<ToT>>{}(context)...);
		}
	};

	template<typename FuncT>
	void CallSystemFunction(SystemContext& context, FuncT&& func)
	{
		using FuncInfo = Potato::TMP::FunctionInfo<std::remove_pointer_t<std::remove_cvref_t<FuncT>>>;
		using Distributors = typename FuncInfo::template PackParameters<ExecuteContextDistributors>;

		Distributors{}(context, func);
	}

	/*
	export template<typename Func>
		struct RunningContextCallableObject : public Holder
	{
		std::conditional_t<
			std::is_function_v<Func>,
			Func*,
			Func
		> fun;

		RunningContextCallableObject(Func&& fun, std::pmr::memory_resource* resource)
			: fun(std::move(fun))
		{

		}
		void Execute(ExecuteContext& con) override
		{
			CallSystemFunction(con, fun);
			//fun(con);
		}
		void Release(std::pmr::memory_resource* resource)
		{
			this->~RunningContextCallableObject();
			resource->deallocate(
				this,
				sizeof(RunningContextCallableObject),
				alignof(RunningContextCallableObject)
			);
		}
	};

	export template<typename Func>
		RunningContext* CreateObjFromCallableObject(Func&& func, std::pmr::memory_resource* resource) requires(
			Potato::TMP::RequireDetectableFunction<Func>
		)
	{
		using ExtractType = Potato::TMP::FunctionInfo<std::remove_pointer_t<std::remove_cvref_t<Func>>>;

		static_assert(
			ExtractType::template PackParameters<IsAcceptableParameters>::value,
			"System Only Accept Parameters with ExecuteContext and ComponentFilter "
		);

		if (resource != nullptr)
		{
			using OT = RunningContextCallableObject<std::remove_cvref_t<Func>>;
			auto adress = resource->allocate(sizeof(OT), alignof(OT));
			if (adress != nullptr)
			{
				return new (adress) OT{ std::forward<Func>(func), resource };
			}
		}
		return {};
	}
	*/

}
