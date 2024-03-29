module;

#include <cassert>

export module NoodlesSystem;

import std;

import PotatoTMP;
import PotatoMisc;
import PotatoPointer;
import PotatoIR;
import PotatoTaskSystem;
import PotatoFormat;


export import NoodlesArchetype;
export import NoodlesComponent;

/*
export namespace Noodles
{
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
		bool operator==(SystemRWInfo const& i) const { return type_id == i.type_id; }

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

	protected:

		virtual bool TryPreCollection(std::size_t element_index, Archetype const& archetype) override;

		SystemComponentFilter(std::span<std::byte> append_buffer, std::size_t allocate_size, std::span<SystemRWInfo const> ref, std::pmr::memory_resource* resource);
		virtual ~SystemComponentFilter();

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
		mutable std::shared_mutex mutex;
		std::pmr::vector<InDirectMapping> in_direct_mapping;
		std::pmr::vector<ArchetypeTypeIDIndex> id_index;

		virtual void AddRef() const override;
		virtual void SubRef() const override;

		friend struct SystemContext;
		friend struct Potato::Pointer::DefaultIntrusiveWrapper;

	public:

		struct Wrapper
		{

			Wrapper(Archetype const& archetype, ArchetypeMountPoint mp, std::span<ArchetypeTypeIDIndex const> id_index, std::span<SystemRWInfo const> infos)
				: archetype(archetype), mp(mp), id_index(id_index), infos(infos)
			{
				
			}
			Wrapper(Wrapper const&) = default;

			std::tuple<void const*, std::size_t> ReadRaw(UniqueTypeID const& ref, std::size_t index) const;

			std::tuple<void*, std::size_t> Get(std::size_t index) const;

			template<typename T>
			std::span<T> GetSpan(std::size_t index) const
			{
				auto [data, count] = Get(index);
				assert(infos.size() > index);
				assert(infos[index].is_write == !std::is_const_v<T>);
				return std::span<T>{
					static_cast<T*>(data),
					count
				};
			}

		protected:

			Archetype const& archetype;
			ArchetypeMountPoint mp;
			std::span<ArchetypeTypeIDIndex const> id_index;
			std::span<SystemRWInfo const> infos;

			friend struct Context;
			friend struct SystemAutomatic;
		};

		template<typename Func>
		bool Foreach(ArchetypeComponentManager& manager, Func&& func) const requires(std::is_invocable_r_v<bool, Func, Wrapper>)
		{
			std::shared_lock sl(mutex);
			for(auto& ite : in_direct_mapping)
			{
				auto span = std::span(id_index).subspan(ite.offset, ref_infos.size());
				auto re = manager.ForeachMountPoint(ite.element_index, [&](ArchetypeMountPointRange range)
				{
					for(auto& ite2 : range)
					{
						Wrapper wrap{range.archetype, ite2, span, ref_infos};
						if(!func(wrap))
						{
							return false;
						}
					}
					return true;
				});
				if(!re)
					return false;
			}
			return true;
		}

		friend struct Context;
	};

	struct SystemEntityFilter : Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<SystemEntityFilter>;

		static auto Create(std::span<SystemRWInfo const> infos, std::pmr::memory_resource* resource)
			-> Ptr;

		struct Wrapper
		{

			Wrapper(Archetype const& archetype, ArchetypeMountPoint mp, std::span<Archetype::Location const> location, std::span<SystemRWInfo const> infos)
				: archetype(archetype), mp(mp), location(location), infos(infos)
			{

			}

			Wrapper(Wrapper const&) = default;

			std::tuple<void const*, std::size_t> ReadRaw(UniqueTypeID const& ref, std::size_t index) const;

			template<typename T>
			std::span<T const> Read(std::size_t index) const
			{
				auto [d, c] = ReadRaw(UniqueTypeID::Create<std::remove_cvref_t<T>>(), index);
				return std::span<T const>{static_cast<T const*>(d), c};
			}

			std::tuple<void*, std::size_t> WriteRaw(UniqueTypeID const& ref, std::size_t index) const;


			template<typename T>
			std::span<T> Write(std::size_t index) const
			{
				auto [d, c] = WriteRaw(UniqueTypeID::Create<std::remove_cvref_t<T>>(), index);
				return std::span<T>{static_cast<T*>(d), c};
			}

		protected:

			Archetype const& archetype;
			ArchetypeMountPoint mp;

			std::span<Archetype::Location const> location;
			std::span<SystemRWInfo const> infos;
		};

		template<typename Func>
		bool ForeachEntity(ArchetypeComponentManager& manager, Entity const& entity, Func&& func, std::pmr::memory_resource* temporary_resource = std::pmr::get_default_resource()) const
			requires(std::is_invocable_r_v<bool, Func, Wrapper>)
		{
			bool re = true;
			auto re2 = manager.ReadEntityMountPoint(entity, [&](EntityStatus status, Archetype const& arc, ArchetypeMountPoint mp)
			{
				std::pmr::vector<Archetype::Location> locations(temporary_resource);
				locations.resize(ref_infos.size());

				for(std::size_t i = 0; i < ref_infos.size(); ++i)
				{
					auto loc = arc.LocateTypeID(ref_infos[i].type_id);
					if(loc.has_value())
					{
						locations[i] = *loc;
					}else
					{
						re = false;
						return;
					}
				}

				Wrapper wra{ arc,  mp, std::span(locations), ref_infos};
				func(wra);
			});

			return re2 && re;
		}

	protected:

		SystemEntityFilter(std::pmr::memory_resource* resource, std::size_t allocated_size,
			std::span<SystemRWInfo const> reference_info, std::span<std::byte> buffer);
		virtual ~SystemEntityFilter();

		virtual void Release() override;
		
		std::span<SystemRWInfo const> ref_infos;
		std::pmr::memory_resource* resource = nullptr;
		std::size_t allocate_size = 0;

		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
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

	enum class SystemCategory
	{
		Normal,
		Parallel,
		FinalParallel,
	};

	struct TickSystemRunningIndex
	{
		std::size_t index;
		std::size_t parameter;
	};

	struct SystemContext;

	export enum class RunningStatus
	{
		PreInit,
		Ready,
		Running,
		Done,
		WaitingParallel,
	};

	export struct SystemHolder : public Potato::Pointer::DefaultIntrusiveInterface
	{

		using Ptr = Potato::Pointer::IntrusivePtr<SystemHolder>;


		template<typename Func, typename AppendData>
		static auto Create(
			Func&& func,
			AppendData&& append_data,
			SystemProperty property,
			std::u8string_view display_prefix,
			std::pmr::memory_resource* resource
		)
			-> Ptr;

		template<typename Func, typename AppendData>
		static auto CreateAuto(
			Func&& func,
			AppendData&& append_data,
			SystemProperty property,
			std::u8string_view display_prefix,
			std::pmr::memory_resource* resource
		)
			-> Ptr;

		virtual void Execute(SystemContext& context) = 0;
		std::u8string_view GetDisplayName() const { return display_name; }
		SystemProperty GetProperty() const { return property; };

		static std::size_t FormatDisplayNameSize(std::u8string_view prefix, SystemProperty property);
		static bool FormatDisplayName(std::span<char8_t> output, std::u8string_view prefix, SystemProperty property);

		SystemHolder(std::span<std::byte> output, std::u8string_view prefix, SystemProperty property);

	protected:

		std::mutex mutex;
		RunningStatus status = RunningStatus::PreInit;
		std::size_t request_parallel = 0;
		std::size_t fast_index = 0;
		SystemProperty property;
		std::u8string_view display_name;

		friend struct TickSystemsGroup;
	};

	struct SystemContext
	{

		SystemContext(SystemContext const&) = default;

		SystemProperty GetProperty() const { return self_property; };

		bool StartParallel(std::size_t parallel_count);

		SystemCategory GetSystemCategory() const { return category; }

		std::size_t GetParallelIndex() const { return parameter; }

		Context* operator->() { return &global_context; }

		SystemContext(SystemContext& ref)
			: ptr(ref.ptr), global_context(ref.global_context), self_property(ref.self_property), category(ref.category), parameter(ref.parameter)
		{
			
		}

	protected:

		SystemContext(SystemHolder& ptr, Context& global_context)
			: ptr(ptr), global_context(global_context)
		{
			
		}

		std::int32_t layer = 0;
		SystemProperty self_property;
		SystemHolder& ptr;
		Context& global_context;
		SystemCategory category = SystemCategory::Normal;
		std::size_t parameter = 0;

		friend struct TickSystemsGroup;
		friend struct Context;
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

		bool StartParallel(SystemHolder& system, std::size_t parallel_count);

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
			Potato::Misc::IndexSpan<> group_name;
			Potato::Misc::IndexSpan<> system_name;
			Potato::Misc::IndexSpan<> component_filter;
			std::pmr::vector<TriggerTo> trigger_to;
			std::size_t in_degree = 0;
			SystemHolder::Ptr system_obj;

			SystemProperty GetProperty(std::u8string_view total) const
			{
				return SystemProperty{
					system_name.Slice(total),
					group_name.Slice(total)
				};
			}

			SystemMutex GetMutex(std::span<SystemRWInfo const> info) const
			{
				return SystemMutex{
				component_filter.Slice(info),
				};
			}
		};

		std::pmr::synchronized_pool_resource system_holder_resource;

		mutable std::shared_mutex graphic_mutex;
		std::pmr::u8string total_string;
		std::pmr::vector<SystemRWInfo> total_rw_info;
		std::pmr::vector<StorageSystemHolder> graphic_node;
		std::pmr::vector<std::size_t> need_destroy_graphic;
		bool need_refresh_dependence = false;

		struct StartupSystem
		{
			std::int32_t layer = 0;
			SystemHolder::Ptr to;
			std::u8string_view display_name;
		};

		struct TriggerLine
		{
			bool is_mutex = false;
			std::size_t to_index;
			SystemHolder::Ptr to;
			std::u8string_view display_name;
		};

		struct SystemRunningContext
		{
			std::size_t startup_in_degree = 0;
			std::size_t current_in_degree = 0;
			std::size_t mutex_degree = 0;
			Potato::Misc::IndexSpan<> reference_trigger_line;
			SystemHolder::Ptr to;
		};

		std::mutex tick_system_running_mutex;
		std::pmr::vector<SystemRunningContext> running_context;
		std::pmr::vector<StartupSystem> startup_system;
		std::pmr::vector<TriggerLine> tick_systems_running_graphic_line;
		std::size_t startup_system_context_ite = 0;
		std::size_t current_level_system_waiting = 0;

		bool SynFlushAndDispatchImp(ArchetypeComponentManager& manager, void(*func)(void* obj, TickSystemRunningIndex, std::u8string_view), void* data);
		bool ExecuteAndDispatchDependence(TickSystemRunningIndex index, Context& context, void(*func)(void* obj, TickSystemRunningIndex, std::u8string_view), void* data);
		void DispatchSystemImp(SystemHolder& system);
		bool StartupNewLayerSystems(void(*func)(void* obj, TickSystemRunningIndex index, std::u8string_view), void* data);

	public:

		bool Empty() const;

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
					std::move(func), std::move(append), property, display_prefix, &system_holder_resource
				);
				Register(layer, property, priority, manager, generator, re, temp_line, std::move(ptr), display_prefix, &system_holder_resource);
			}
			return re;
		}

		template<typename Func>
		SystemRegisterResult RegisterAutoDefer(ArchetypeComponentManager& manager, std::int32_t layer, SystemPriority priority, SystemProperty property,
			Func&& func, std::u8string_view display_prefix = {}, std::pmr::memory_resource* temporary_resource = std::pmr::get_default_resource());

		template<typename GeneratorFunc, typename Func>
		bool RegisterTemplateFunction();

		//void ExecuteSystem(std::size_t index, ArchetypeComponentManager& manager, Context& context);

		template<typename Func>
		bool SynFlushAndDispatch(ArchetypeComponentManager& manager, Func&& func) requires(std::is_invocable_v<Func, TickSystemRunningIndex, std::u8string_view>)
		{
			return SynFlushAndDispatchImp(manager, [](void* data, TickSystemRunningIndex ptr, std::u8string_view str)
			{
				(*static_cast<Func*>(data))(ptr, str);
			}, &func);
		}

		template<typename Func>
		std::optional<std::size_t> ExecuteAndDispatchDependence(TickSystemRunningIndex index, Context& context,  Func&& func) requires(std::is_invocable_v<Func, TickSystemRunningIndex, std::u8string_view>)
		{
			return ExecuteAndDispatchDependence(index, context, [](void* data, TickSystemRunningIndex index, std::u8string_view str)
				{
					(*static_cast<Func*>(data))(index, str);
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

		DynamicSystemHolder(AppendData&& append_data, Func&& fun, std::span<std::byte> output, std::u8string_view prefix, SystemProperty in_property, std::pmr::memory_resource* resource)
			: SystemHolder(output, prefix, in_property), append_data(std::move(append_data)), fun(std::move(fun)), resource(resource)
		{}

		virtual void Execute(SystemContext& context) override { fun(context, append_data); }

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
		SystemProperty property,
		std::u8string_view display_prefix,
		std::pmr::memory_resource* resource
	)
		-> Ptr
	{
		using Type = DynamicSystemHolder<std::remove_cvref_t<AppendData>, std::remove_cvref_t<Func>>;

		if(resource != nullptr)
		{
			std::size_t dis_size = SystemHolder::FormatDisplayNameSize(display_prefix, property);
			std::size_t append_size = dis_size * sizeof(char8_t);

			if((append_size % alignof(Type)) != 0)
			{
				append_size += alignof(Type) - (append_size % alignof(Type));
			}
			auto buffer = resource->allocate(sizeof(Type) + append_size, alignof(Type));
			if(buffer != nullptr)
			{
				std::span<std::byte> str{static_cast<std::byte*>(buffer) + sizeof(Type), append_size };

				Type* ptr = new (buffer) Type(
					std::forward<AppendData>(append_data),
					std::forward<Func>(func),
					str,
					display_prefix,
					property,
					resource
				);
				return Ptr{ptr};
			}
		}
		return {};
	}


	template<typename ParT>
	concept DetectSystemHasGenerateFilter = requires(ParT)
	{
		{ParT::GenerateFilter(std::declval<FilterGenerator&>())};
		!std::is_same_v<decltype(ParT::GenerateFilter(std::declval<FilterGenerator&>())), void>;
	};

	struct SystemAutomatic
	{

		struct Holder {};

		template<typename ParT>
		struct ExtractAppendDataForParameter
		{
			using Type = std::conditional_t<std::is_same_v<ParT, SystemContext>, Holder, ParT>;
			static Type Generate(FilterGenerator& Generator) { return {}; }
			template<typename ParT2>
				requires(std::is_same_v<Type, Holder>)
			static decltype(auto) Translate(SystemContext& context, ParT2&& par2) { return context; }
			template<typename ParT2>
				requires(!std::is_same_v<Type, Holder>)
			static decltype(auto) Translate(SystemContext& context, ParT2&& par2) { return par2; }
		};


		template<DetectSystemHasGenerateFilter ParT>
		struct ExtractAppendDataForParameter<ParT>
		{
			using Type = decltype(ParT::GenerateFilter(std::declval<FilterGenerator&>()));
			static Type Generate(FilterGenerator& Generator) { return ParT::GenerateFilter(Generator); }
			template<typename ParT2>
			static decltype(auto) Translate(SystemContext& context, ParT2&& par2) { return par2; }
		};

		template<typename ...ParT>
		struct ExtractAppendDataForParameters
		{

			using Type = std::tuple<
				typename ExtractAppendDataForParameter<std::remove_cvref_t<ParT>>::Type...
			>;

			static Type Construct(FilterGenerator& generator)
			{
				return Type{
					ExtractAppendDataForParameter<std::remove_cvref_t<ParT>>::Generate(generator)...
				};
			}

			using Index = std::make_index_sequence<sizeof...(ParT)>;
		};

		template<typename Func>
		struct ExtractTickSystem
		{
			using Extract = typename Potato::TMP::FunctionInfo<std::remove_cvref_t<Func>>::template PackParameters<ExtractAppendDataForParameters>;

			using AppendDataT = typename Extract::Type;

			static AppendDataT Generate(FilterGenerator& generator){ return Extract::Construct(generator); }
			static auto Execute(SystemContext& context, AppendDataT& append_data, Func& func)
			{
				return std::apply([&](auto&& ...par)
				{
					func(
						ExtractAppendDataForParameter<std::remove_cvref_t<decltype(par)>>::Translate(context, par)...
					);
				}, append_data);
			}
		};

		template<std::size_t i>
		struct ApplySingleFilter
		{
			template<typename ...OutputTuple>
			static void Apply(SystemComponentFilter::Wrapper const& filter, std::tuple<std::span<OutputTuple>...>& output)
			{
				using Type = typename std::tuple_element_t<i - 1, std::tuple<std::span<OutputTuple>...>>::element_type;
				std::get<i - 1>(output) = filter.GetSpan<Type>(i - 1);
				ApplySingleFilter<i - 1>::Apply(filter, output);
			}

			template<typename ...OutputTuple>
			static void Apply(Archetype const& arc, ArchetypeMountPoint mp, std::tuple<std::span<OutputTuple>...>& output)
			{
				using Type = typename std::tuple_element_t<i - 1, std::tuple<std::span<OutputTuple>...>>::element_type;
				auto loc = arc.LocateTypeID(UniqueTypeID::Create<std::remove_cvref_t<Type>>());
				assert(loc);
				auto data = arc.GetData(loc->index, 0, mp);
				std::get<i - 1>(output) = {
					static_cast<Type*>(data),
					loc->count
				};
				ApplySingleFilter<i - 1>::Apply(arc, mp, output);
			}
		};


		template<>
		struct ApplySingleFilter<0>
		{
			template<typename ...OutputTuple>
			static void Apply(SystemComponentFilter::Wrapper const& filter, std::tuple<std::span<OutputTuple>...>& output) {}
			template<typename ...OutputTuple>
			static void Apply(Archetype const& arc, ArchetypeMountPoint mp, std::tuple<std::span<OutputTuple>...>& output) {}
		};
	};

	template<typename Func>
	struct DynamicAutoSystemHolder : public SystemHolder
	{
		using Automatic = SystemAutomatic::ExtractTickSystem<Func>;

		typename Automatic::AppendDataT append_data;

		std::conditional_t<
			std::is_function_v<Func>,
			Func*,
			Func
		> fun;

		std::pmr::memory_resource* resource;

		DynamicAutoSystemHolder(Automatic::AppendDataT&& append_data, Func&& fun, std::span<std::byte> output, std::u8string_view prefix, SystemProperty in_property, std::pmr::memory_resource* resource)
			: SystemHolder(output, prefix, in_property), append_data(std::move(append_data)), fun(std::move(fun)), resource(resource)
		{}

		virtual void Execute(SystemContext& context) override
		{
			Automatic::Execute(context, append_data, fun);
		}

		virtual void Release() override
		{
			auto o_res = resource;
			this->~DynamicAutoSystemHolder();
			o_res->deallocate(
				this,
				sizeof(DynamicAutoSystemHolder),
				alignof(DynamicAutoSystemHolder)
			);
		}
	};

	template<typename Func, typename AppendData>
	auto SystemHolder::CreateAuto(
		Func&& func,
		AppendData&& append_data,
		SystemProperty property,
		std::u8string_view display_prefix,
		std::pmr::memory_resource* resource
	)
		-> Ptr
	{
		using Type = DynamicAutoSystemHolder<std::remove_cvref_t<Func>>;

		if (resource != nullptr)
		{
			std::size_t dis_size = SystemHolder::FormatDisplayNameSize(display_prefix, property);
			std::size_t append_size = dis_size * sizeof(char8_t);

			if ((append_size % alignof(Type)) != 0)
			{
				append_size += alignof(Type) - (append_size % alignof(Type));
			}
			auto buffer = resource->allocate(sizeof(Type) + append_size, alignof(Type));
			if (buffer != nullptr)
			{
				std::span<std::byte> str{ static_cast<std::byte*>(buffer) + sizeof(Type), append_size };

				Type* ptr = new (buffer) Type(
					std::forward<AppendData>(append_data),
					std::forward<Func>(func),
					str,
					display_prefix,
					property,
					resource
				);
				return Ptr{ ptr };
			}
		}
		return {};
	}


	template<typename Func>
	SystemRegisterResult TickSystemsGroup::RegisterAutoDefer(ArchetypeComponentManager& manager, std::int32_t layer, SystemPriority priority, SystemProperty property,
		Func&& func, std::u8string_view display_prefix, std::pmr::memory_resource* temporary_resource)
	{
		using Type = SystemAutomatic::ExtractTickSystem<Func>;
		std::lock_guard lg(graphic_mutex);
		std::pmr::monotonic_buffer_resource temp_resource(temporary_resource);
		FilterGenerator generator(&temp_resource, &system_holder_resource);
		auto append = Type::Generate(generator);
		std::pmr::vector<SystemTemporaryDependenceLine> temp_line{ &temp_resource };
		auto re = PreRegisterCheck(layer, priority, property, generator.GetMutex(), temp_line);
		if (re)
		{
			auto ptr = SystemHolder::CreateAuto(
				std::move(func), std::move(append), property, display_prefix, &system_holder_resource
			);
			Register(layer, property, priority, manager, generator, re, temp_line, std::move(ptr), display_prefix, &system_holder_resource);
		}
		return re;
	}

}
*/
