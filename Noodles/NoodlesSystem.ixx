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
	/*
	struct Context;

	struct SystemRWInfo
	{

		bool is_write = false;
		UniqueTypeID type_id;

		template<typename Type>
		static SystemRWInfo Create()
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

	struct SystemComponentFilter : public ComponentFilterInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<SystemComponentFilter>;

		static Ptr Create(
			std::span<SystemRWInfo const> inf,
			std::pmr::memory_resource* upstream = std::pmr::get_default_resource()
		);

		template<typename RequireType>
		RequireType const* ReadData(Archetype& arc, ArchetypeMountPoint mp, std::size_t fast_cache)
		{
			auto adress = ReadDataRaw(arc, mp, UniqueTypeID::Create<RequireType>(), fast_cache);
			if(adress != nullptr)
			{
				return static_cast<RequireType const*>(adress);
			}
			return nullptr;
		}

		void* ReadDataRaw(Archetype& arc, ArchetypeMountPoint mp, UniqueTypeID const& require_id, std::size_t fast_cache);

	protected:

		virtual bool TryPreCollection(std::size_t element_index, Archetype const& archetype) override;

		SystemComponentFilter(std::span<std::byte> append_buffer, std::size_t allocate_size, std::span<SystemRWInfo const> ref, std::pmr::memory_resource* resource);
		virtual ~SystemComponentFilter() override;

		mutable Potato::Misc::AtomicRefCount Ref;
		std::size_t allocate_size = 0;
		std::pmr::memory_resource* resource = nullptr;

		struct InDirectMapping
		{
			std::size_t element_index;
			std::size_t offset;
		};

		struct ArchetypeTypeIDIndex
		{
			std::size_t index = 0;
			std::size_t count = 0;
		};

		std::span<SystemRWInfo const> ref_infos;
		std::shared_mutex mutex;
		std::pmr::vector<InDirectMapping> in_direct_mapping;
		std::pmr::vector<ArchetypeTypeIDIndex> id_index;

		virtual void AddRef() const override;
		virtual void SubRef() const override;

		friend struct SystemContext;
		friend struct Potato::Pointer::IntrusiveSubWrapperT;

	public:
		
		struct Iterator
		{

			struct Block
			{
				std::size_t element_index;
				std::span<ArchetypeTypeIDIndex const> index_span;
			};

			Block operator*() const
			{
				return { iterator->element_index, index_span.subspan(iterator->offset, block_size) };
			}
			Iterator& operator++() { ++iterator; return *this; }
			std::strong_ordering operator<=>(Iterator const& i1) const { return iterator <=> i1.iterator; }

			Iterator(Iterator const&) = default;
			Iterator& operator=(Iterator const&) = default;

		protected:

			Iterator(decltype(in_direct_mapping)::iterator ite, std::span<ArchetypeTypeIDIndex const> span, std::size_t b_size)
				: iterator(ite), index_span(span), block_size(b_size)
			{
			}

			//ArchetypeComponentManager& manager;
			decltype(in_direct_mapping)::iterator iterator;
			std::span<ArchetypeTypeIDIndex const> index_span;
			std::size_t block_size;

			friend struct SystemComponentFilter;

		};

		Iterator begin()
		{
			return Iterator{in_direct_mapping.begin(), std::span<ArchetypeTypeIDIndex const>(id_index), ref_infos.size()};
		}

		Iterator end()
		{
			return Iterator{ in_direct_mapping.begin(), std::span<ArchetypeTypeIDIndex const>(id_index), ref_infos.size() };
		}

	};

	struct SystemEntityFilter : Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<SystemEntityFilter>;

		static auto Create(std::span<SystemRWInfo const> infos, std::pmr::memory_resource* resource)
			-> Ptr;

	protected:

		SystemEntityFilter(std::pmr::memory_resource* resource, std::size_t allocated_size,
			std::span<SystemRWInfo const> reference_info, std::span<std::byte> buffer);
		virtual ~SystemEntityFilter();

		virtual void Release() override;
		
		std::span<SystemRWInfo const> ref_infos;
		std::pmr::memory_resource* resource = nullptr;
		std::size_t allocate_size = 0;

		friend struct Potato::Pointer::IntrusiveSubWrapperT;
	};


	struct FilterGenerator
	{

		SystemComponentFilter::Ptr CreateComponentFilter(std::span<SystemRWInfo const> ifs);
		SystemEntityFilter::Ptr CreateEntityFilter(std::span<SystemRWInfo const> ifs);
		SystemMutex GetMutex() const { return {std::span(component_rw_infos)}; }

	protected:

		void FinshConstruction();

		FilterGenerator(std::pmr::memory_resource* template_resource, std::pmr::memory_resource* system_resource)
			: component_rw_infos(template_resource), system_resource(system_resource) { }

		std::pmr::vector<SystemRWInfo> component_rw_infos;
		std::pmr::vector<SystemComponentFilter::Ptr> need_register_component;
		std::pmr::memory_resource* temp_memory_resource;
		std::pmr::memory_resource* system_resource;

		friend struct TickSystemsGroup;
	};

	struct SystemContext
	{
		std::int32_t layer;
		SystemProperty self_property;
		ArchetypeComponentManager& manager;
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

	
		template<typename Func, typename AppendData>
		static auto Create(
			Func&& func,
			AppendData&& appendData,
			std::pmr::memory_resource* resource
			)
		->Ptr;

		virtual void Execute(ArchetypeComponentManager&, Context&, std::int32_t layer, SystemProperty property) = 0;
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

		SystemRegisterResult PreRegisterCheck(std::int32_t layer, SystemPriority priority, SystemProperty property, SystemMutex const& pro, std::pmr::vector<SystemTemporaryDependenceLine>& dep);

		void Register(
			std::int32_t layer, SystemProperty property, SystemPriority priority, ArchetypeComponentManager& manager,
			FilterGenerator const& generator, SystemRegisterResult const& result, 
			std::pmr::vector<SystemTemporaryDependenceLine> const& d_line, SystemHolder::Ptr ptr,
			std::u8string_view display_name_proxy,
			std::pmr::memory_resource* resource
			);

		struct TriggerTo
		{
			bool is_mutex = true;
			std::size_t to = 0;
		};

		struct StorageSystemHolder
		{
			std::int32_t layer = 0;
			SystemPriority priority;
			std::pmr::u8string total_string;
			Potato::Misc::IndexSpan<> group_name;
			Potato::Misc::IndexSpan<> system_name;
			Potato::Misc::IndexSpan<> display_name;
			std::pmr::vector<SystemRWInfo> filter_info;
			Potato::Misc::IndexSpan<> component_filter;
			std::pmr::vector<TriggerTo> trigger_to;
			std::size_t in_degree = 0;
			SystemHolder::Ptr system_obj;

			SystemProperty GetProperty() const
			{
				return SystemProperty{
				system_name.Slice(std::u8string_view{total_string}),
					group_name.Slice(std::u8string_view{total_string})
				};
			}

			SystemMutex GetMutex() const
			{
				return SystemMutex{
				component_filter.Slice(std::span(filter_info)),
				};
			}
		};

		std::pmr::synchronized_pool_resource system_holder_resource;

		std::shared_mutex graphic_mutex;
		std::pmr::vector<StorageSystemHolder> graphic_node;
		std::pmr::vector<std::size_t> need_destroy_graphic;
		bool need_refresh_dependence = false;

		struct StartupSystem
		{
			std::int32_t layer = 0;
			std::size_t index = 0;
			std::u8string_view display_name;
		};

		struct SystemRunningContext
		{
			RunningStatus status = RunningStatus::PreInit;
			std::size_t startup_in_degree = 0;
			std::size_t current_in_degree = 0;
			std::size_t mutex_degree = 0;
			Potato::Misc::IndexSpan<> reference_trigger_line;
			SystemHolder::Ptr system_obj;
		};

		struct TriggerLine
		{
			bool is_mutex = false;
			std::size_t index = 0;
			std::u8string_view display_name;
		};

		std::shared_mutex tick_system_running_mutex;
		std::pmr::vector<SystemRunningContext> system_contexts;
		std::pmr::vector<StartupSystem> startup_system;
		std::size_t startup_system_context_ite = 0;
		std::pmr::vector<TriggerLine> tick_systems_running_graphic_line;
		std::size_t current_level_system_waiting = 0;

		void FlushAndInitRegisterSystem(ArchetypeComponentManager& manager);

		std::size_t SynFlushAndDispatchImp(ArchetypeComponentManager& manager, void(*func)(void* obj, std::size_t, std::u8string_view), void* data);
		std::optional<std::size_t> TryDispatchDependenceImp(std::size_t, void(*func)(void* obj, std::size_t, std::u8string_view), void* data);
		void DispatchSystemImp(std::size_t index);

	public:

		template<typename GeneratorFunc, typename Func>
		SystemRegisterResult RegisterDefer(ArchetypeComponentManager& manager, std::int32_t layer, SystemPriority priority, SystemProperty property,
			GeneratorFunc&& g_func, Func&& func, std::u8string_view display_prefix = {}, std::pmr::memory_resource* temporary_resource = std::pmr::get_default_resource())
			requires(
				std::is_invocable_v<GeneratorFunc, FilterGenerator&> 
				&& !std::is_same_v<decltype(g_func(std::declval<FilterGenerator&>())), void> &&
				std::is_invocable_v<Func, SystemContext&, std::remove_cvref_t<decltype(g_func(std::declval<FilterGenerator&>()))>&>
			)
		{
			std::lock_guard lg(graphic_mutex);
			std::pmr::monotonic_buffer_resource temp_resource(temporary_resource);
			FilterGenerator generator(&temp_resource, &system_holder_resource);
			auto append = g_func(generator);
			std::pmr::vector<SystemTemporaryDependenceLine> temp_line{&temp_resource };
			auto re = PreRegisterCheck(layer, priority, property, generator.GetMutex(), temp_line);
			if(re)
			{
				auto ptr = SystemHolder::Create(
					std::move(func), std::move(append), &system_holder_resource
				);
				Register(layer, property, priority, manager, generator, re, temp_line, std::move(ptr), display_prefix, &system_holder_resource);
			}
			return re;
		}

		void ExecuteSystem(std::size_t index, ArchetypeComponentManager& manager, Context& context);

		template<typename Func>
		std::size_t SynFlushAndDispatch(ArchetypeComponentManager& manager, Func&& func) requires(std::is_invocable_v<Func, std::size_t, std::u8string_view>)
		{
			return SynFlushAndDispatchImp(manager, [](void* data, std::size_t ptr, std::u8string_view str)
			{
				(*static_cast<Func*>(data))(ptr, str);
			}, &func);
		}

		template<typename Func>
		std::optional<std::size_t> TryDispatchDependence(std::size_t index, Func&& func) requires(std::is_invocable_v<Func, std::size_t, std::u8string_view>)
		{
			return TryDispatchDependenceImp(index, [](void* data, std::size_t ptr, std::u8string_view str)
				{
					(*static_cast<Func*>(data))(ptr, str);
				}, &func);
		}
	};

	template<typename AppendData, typename Func>
	struct DynamicSystemHolder : public SystemHolder
	{
		AppendData append_data;
		std::conditional_t<
			std::is_function_v<Func>,
			Func*,
			Func
		> fun;

		std::pmr::memory_resource* resource;

		DynamicSystemHolder(AppendData&& append_data, Func&& fun, std::pmr::memory_resource* resource)
			: append_data(std::move(append_data)), fun(std::move(fun)), resource(resource)
		{

		}

		virtual void Execute(ArchetypeComponentManager& manager, Context& context, std::int32_t layer, SystemProperty property) override
		{
			SystemContext sys_context
			{
				layer,
				property,
				manager,
				context
			};

			fun(sys_context, append_data);
		}

		virtual void Release() override
		{
			auto o_res = resource;
			this->~DynamicSystemHolder();
			o_res->deallocate(
				this,
				sizeof(DynamicSystemHolder),
				alignof(DynamicSystemHolder)
			);
		}
	};

	template<typename Func, typename AppendData>
	auto SystemHolder::Create(
		Func&& func,
		AppendData&& append_data,
		std::pmr::memory_resource* resource
	)
		-> Ptr
	{
		using Type = DynamicSystemHolder<std::remove_cvref_t<AppendData>, std::remove_cvref_t<Func>>;
		if(resource != nullptr)
		{
			auto buffer = resource->allocate(sizeof(Type), alignof(Type));
			if(buffer != nullptr)
			{
				Type* ptr = new (buffer) Type{ std::forward<AppendData>(append_data), std::forward<Func>(func), resource};
				return Ptr{ptr};
			}
		}
		return {};
	}



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
	*/
}
