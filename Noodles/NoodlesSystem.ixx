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
}


export namespace Noodles::System
{

	struct RWInfo
	{

		bool is_write = false;
		Potato::IR::TypeID type_id;

		template<typename Type>
		static RWInfo GetComponent()
		{
			return RWInfo{
				!std::is_const_v<Type>,
				Potato::IR::TypeID::CreateTypeID<std::remove_cvref_t<Type>>()
			};
		}

		std::strong_ordering operator<=>(RWInfo const& i) const { type_id <=> i.type_id; } 

	};

	struct Property;

	struct Priority
	{
		std::int32_t primary_priority = 0;
		std::int32_t second_priority = 0;
		std::partial_ordering (*compare)(Property const& self, Property const& target) = nullptr;

		std::strong_ordering ComparePriority(Priority const& p2) const;
		std::partial_ordering CompareCustomPriority(Property const& self_property, Priority const& target, Property const& target_property) const;
	};

	struct Property
	{
		std::u8string_view system_name;
		std::u8string_view group_name;
		
		bool IsSameSystem(Property const& oi) const
		{
			return group_name == oi.group_name && system_name == oi.system_name;
		}
	};

	struct MutexProperty
	{
		std::span<RWInfo const> component_rw_infos;
		std::span<RWInfo const> global_component_rw_infos;

		bool IsConflict(MutexProperty const& p2) const;
	};

	export struct FilterGenerator
	{
		enum class Type
		{
			Component,
			GlobalComponent,
			Entity,
		};

		FilterGenerator(std::pmr::memory_resource* ptr = std::pmr::get_default_resource());

		void AddComponentFilter(std::span<RWInfo> infos);
		void AddEntityFilter(std::span<RWInfo> infos);
		void AddGlobalComponentFilter(RWInfo const& info);

		MutexProperty GetMutexProperty() const
		{
			return {
				std::span(component_rw_info),
				std::span(global_component_rw_info)
				};
		}

		template<typename FunctionInfo>
		static FilterGenerator Create(std::pmr::memory_resource* resource);

	protected:

		struct Element
		{
			Type type;
			std::pmr::vector<RWInfo> info;
		};

		std::pmr::memory_resource* resource;
		std::pmr::vector<Element> filter_element;
		std::pmr::vector<RWInfo> component_rw_info;
		std::pmr::vector<RWInfo> global_component_rw_info;
	};

	export struct FilterWrapper : Potato::Task::ControlDefaultInterface
	{
		using Ptr = Potato::Task::ControlPtr<FilterWrapper>;
		using WPtr = Potato::Pointer::IntrusivePtr<FilterWrapper>;

		virtual MutexProperty GetMutexProperty() const = 0;
	public:
	};

	struct ExecuteContext
	{
		System::Property property;
		std::span<FilterWrapper::Ptr> filter_ptr;
		Context& context;
	};

	export enum class RunningStatus
	{
		PreInit,
		Waiting,

		Ready,
		Running,
		Done,
	};

	export struct Holder : public Potato::Pointer::DefaultIntrusiveInterface
	{

		using Ptr = Potato::Pointer::IntrusivePtr<Holder>;

		Property GetProperty() const {
			return {
				std::u8string_view{group_name},
				std::u8string_view{ system_name }
			};
		}

		std::u8string_view GetDisplayName() const { return std::u8string_view{ display_name }; }

		MutexProperty GetMutexProperty() const
		{
			return {
				std::span(component_rw_info),
				std::span(global_component_rw_info)
			};
		};

		template<typename Func>
		static auto Create(
			Func&& func,
			Priority const& priority,
			Property const& property,
			FilterGenerator const& generator,
			std::pmr::memory_resource* resource
			)
		->Ptr;

		virtual void Execute(Context&) = 0;

	protected:

		Holder(
			//Priority const& priority,
			Property const& property,
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
		std::vector<RWInfo> component_rw_info;
		std::vector<RWInfo> global_component_rw_info;
	};

	export struct TriggerLine
	{
		bool is_mutex = false;
		Holder::Ptr target = nullptr;
	};

	struct TemporaryDependenceLine
	{
		bool is_mutex;
		std::size_t from;
		std::size_t to;
	};

	export struct LoginSystemResult
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


		template<typename Func>
		LoginSystemResult LoginDefer(std::int32_t layer, Priority priority, Property property,
			Func&& func,
			std::pmr::memory_resource* resource
		);

		template<typename Func>
		LoginSystemResult LoginDefer(std::int32_t layer, Priority priority, Property property,
			FilterGenerator const& generator,
			Func&& func,
			std::pmr::memory_resource* resource
			);

		template<typename Func>
		std::size_t SynFlushAndDispatch(Func&& func) requires(std::is_invocable_v<Func, Holder::Ptr&>);

		template<typename Func>
		std::optional<std::size_t> TryDispatchDependence(Func&& func) requires(std::is_invocable_v<Func, Holder::Ptr&>);

	protected:

		LoginSystemResult PreLoginCheck(std::int32_t layer, Priority priority, Property property, MutexProperty const& pro, std::pmr::vector<TemporaryDependenceLine>& dep);
		void Login(LoginSystemResult const& Result, std::pmr::vector<TemporaryDependenceLine> const&, Holder::Ptr TargetPtr);


		struct GraphicNode
		{
			struct TriggerTo
			{
				bool is_mutex;
				std::size_t to;
			};

			std::int32_t layer = 0;
			Priority priority;
			Property property;
			MutexProperty mutex_property;
			std::pmr::vector<TriggerTo> trigger_to;
			std::size_t in_degree = 0;
			Holder::Ptr system_obj;
		};

		std::pmr::synchronized_pool_resource system_holder_resource;

		std::mutex graphic_mutex;
		std::pmr::vector<GraphicNode> graphic_node;
		bool need_refresh_dependence = false;

		struct StartupSystem
		{
			std::int32_t layout = 0;
			Holder::Ptr system_obj;
		};

		std::mutex tick_system_running_mutex;
		std::pmr::vector<StartupSystem> startup_system_context;
		std::size_t startup_system_context_ite = 0;
		std::pmr::vector<TriggerLine> tick_systems_running_graphic_line;
		std::size_t current_level_system_waiting = 0;
	};

	template<typename Func>
	LoginSystemResult TickSystemsGroup::LoginDefer(std::int32_t layer, Priority priority, Property property,
		FilterGenerator const& generator,
		Func&& func,
		std::pmr::memory_resource* resource
	)
	{
		std::lock_guard lg(graphic_mutex);
		auto mutex_pro = generator.GetMutexProperty();
		std::pmr::vector<TemporaryDependenceLine> new_dep(resource);
		auto result = PreLoginCheck(layer, priority, property, mutex_pro, new_dep);
		if(result)
		{
			auto ptr = Holder::Create(std::forward<Func>(func), priority, property, generator, &system_holder_resource);
			if(ptr)
			{
				Login(result, new_dep, std::move(ptr));
			}
		}
		return result;
	}

	template<typename Func>
	LoginSystemResult TickSystemsGroup::LoginDefer(std::int32_t layer, Priority priority, Property property,
		Func&& func,
		std::pmr::memory_resource* resource
	)
	{
		auto generator = FilterGenerator::Create<Potato::TMP::FunctionInfo<std::remove_cvref_t<Func>>>(resource);
		return TickSystemsGroup::LoginDefer(layer, priority, property, generator, std::forward<Func>(func), resource);
	}


	template<typename Func>
	concept AcceptableSystemObject = true;

	template<typename ParT>
	struct IsAcceptableParameter
	{
		using PT = std::remove_cvref_t<ParT>;
		static constexpr bool value = std::is_same_v<ExecuteContext, PT> || IsAcceptableComponentFilterV<ParT>;
	};

	template<typename ...ParT>
	struct IsAcceptableParameters
	{
		static constexpr bool value = (true && ... && IsAcceptableParameter<ParT>::value);
	};

	export template<typename ToT>
		struct ExecuteContextDistributor
	{
		ToT operator()(ExecuteContext& context) = delete;
	};

	export template<>
		struct ExecuteContextDistributor<ExecuteContext>
	{
		ExecuteContext& operator()(ExecuteContext& context) { return context; }
	};

	export template<typename ...ToT>
		struct ExecuteContextDistributors
	{
		template<typename Func>
		void operator()(ExecuteContext& context, Func&& func)
		{
			func(ExecuteContextDistributor<std::remove_cvref_t<ToT>>{}(context)...);
		}
	};

	template<typename FuncT>
	void CallSystemFunction(ExecuteContext& context, FuncT&& func)
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
