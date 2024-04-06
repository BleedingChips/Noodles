module;

export module NoodlesContext;

import std;

import PotatoMisc;
import PotatoPointer;
import PotatoIR;
import PotatoTaskSystem;
import PotatoTaskFlow;
import PotatoTMP;


import NoodlesMemory;
import NoodlesArchetype;
import NoodlesComponent;



export namespace Noodles
{

	struct RWUniqueTypeID
	{
		bool is_write = false;
		UniqueTypeID type_id;
		template<typename Type>
		static RWUniqueTypeID Create()
		{
			return RWUniqueTypeID{
				!std::is_const_v<Type>,
				UniqueTypeID::Create<std::remove_cvref_t<Type>>()
			};
		}
	};

	struct Priority
	{
		std::int32_t primary = 0;
		std::int32_t second = 0;
		std::strong_ordering operator<=>(const Priority&) const = default;
		bool operator==(const Priority&) const = default;
	};

	struct Property
	{
		std::u8string_view name;
		std::u8string_view group;
		bool operator==(const Property& i1) const { return name == i1.name && group == i1.group; }
	};

	struct ReadWriteMutex
	{
		std::span<RWUniqueTypeID> components;
		std::span<RWUniqueTypeID> singleton;
		std::optional<UniqueTypeID> system;

		bool IsConflict(ReadWriteMutex const& mutex) const;
	};

	export struct Context;

	struct ReadWriteMutexGenerator
	{

		void RegisterComponentMutex(std::span<RWUniqueTypeID const> ifs);
		void RegisterSingletonMutex(std::span<RWUniqueTypeID const> ifs);
		void SetSystemRWUniqueID();
		std::tuple<std::size_t, std::size_t> CalculateUniqueIDCount() const;

	protected:

		ReadWriteMutexGenerator(std::pmr::memory_resource* template_resource) { }


		struct Tuple
		{
			RWUniqueTypeID unique_id;
			bool is_component = false;
		};

		std::pmr::vector<Tuple> unique_ids;
		std::optional<UniqueTypeID> system_id;

		friend struct Context;
	};

	

	struct ExecuteContext
	{
		Potato::Task::TaskContext& task_context;
		Context& noodles_context;
	};

	export struct SystemHolder : protected Potato::Task::TaskFlowNode, protected Potato::Pointer::DefaultIntrusiveInterface
	{

		using Ptr = Potato::Pointer::IntrusivePtr<SystemHolder, Potato::Task::TaskFlowNode::Wrapper>;


		template<typename Func, typename AppendData>
		static auto Create(
			Func&& func,
			AppendData&& append_data,
			Property property,
			std::u8string_view display_prefix,
			std::pmr::memory_resource* resource
		)
			-> Ptr;

		template<typename Func, typename AppendData>
		static auto CreateAuto(
			Func&& func,
			AppendData&& append_data,
			Property property,
			std::u8string_view display_prefix,
			std::pmr::memory_resource* resource
		)
			-> Ptr;

		virtual void TaskFlowNodeExecute(Potato::Task::TaskFlowStatus& status) override final;
		virtual void SystemExecute(ExecuteContext& context) = 0;

		std::u8string_view GetDisplayName() const { return display_name; }
		Property GetProperty() const { return property; };

		static std::size_t FormatDisplayNameSize(std::u8string_view prefix, Property property);
		static std::optional<std::tuple<std::u8string_view, Property>> FormatDisplayName(std::span<char8_t> output, std::u8string_view prefix, Property property);

		SystemHolder(Property property, std::u8string_view display_name)
			: property(property), display_name(display_name) {}

	protected:

		Property property;
		std::u8string_view display_name;

		void AddTaskFlowNodeRef() const override { DefaultIntrusiveInterface::AddRef(); }
		void SubTaskFlowNodeRef() const override { DefaultIntrusiveInterface::SubRef(); }

		friend struct Potato::Task::TaskFlowNode::Wrapper;
		friend struct Context;
	};

	export struct Context : protected Potato::Task::TaskFlow, protected Potato::Pointer::DefaultIntrusiveInterface
	{
		struct Config
		{
			std::chrono::milliseconds min_frame_time = std::chrono::milliseconds{ 13 };
		};

		using OrderFunction = std::partial_ordering(*)(Property p1, Property p2);

		using Ptr = Potato::Pointer::IntrusivePtr<Context, Potato::Pointer::DefaultIntrusiveWrapper>;

		static Ptr Create(Config config = {}, std::u8string_view name = u8"Noodles Default Context", std::pmr::memory_resource* resource = std::pmr::get_default_resource());
		template<typename ...AT>
		EntityPtr CreateEntityDefer(AT&& ...at) { return manager.CreateEntityDefer(std::forward<AT>(at)...); }

		bool Commit(Potato::Task::TaskContext& context, Potato::Task::TaskProperty property);

		template<typename Func>
		bool CreateTickSystemAuto(std::int32_t layer, Priority priority, Property property,
			Func&& func, OrderFunction order_func = nullptr, Potato::Task::TaskProperty task_property = {}, std::pmr::memory_resource* temporary_resource = std::pmr::get_default_resource());

	protected:

		bool RegisterSystem(SystemHolder::Ptr, std::int32_t layer, Priority priority, Property property, OrderFunction func, Potato::Task::TaskProperty task_property, ReadWriteMutexGenerator& generator);
		Context(Config config, std::u8string_view name, Potato::IR::MemoryResourceRecord record) noexcept : config(config), name(name), record(record), manager(record.GetResource()){};

		void AddTaskRef() const override;
		void SubTaskRef() const override;
		void Release() override;
		void OnBeginTaskFlow(Potato::Task::ExecuteStatus& status) override;
		void OnFinishTaskFlow(Potato::Task::ExecuteStatus& status) override;

		Potato::IR::MemoryResourceRecord record;
		std::u8string_view name;
		std::mutex mutex;
		Config config;
		bool require_quit = false;
		std::optional<std::chrono::steady_clock::time_point> start_up_tick_lock;
		ArchetypeComponentManager manager;

		std::mutex system_mutex;
		struct SystemTuple
		{
			SystemHolder::Ptr system;
			Property property;
			Priority priority;
			Potato::Misc::IndexSpan<> component_index;
			Potato::Misc::IndexSpan<> singleton_index;
			OrderFunction order_function;
		};
		std::pmr::vector<SystemTuple> systems;
		std::pmr::vector<RWUniqueTypeID> rw_unique_id;


		std::pmr::synchronized_pool_resource system_resource;

		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
	};

	template<typename ...ComponentT>
	struct ComponentFilter : protected ComponentFilterInterface, protected Potato::Pointer::DefaultIntrusiveInterface
	{

		static_assert(!Potato::TMP::IsRepeat<ComponentT...>::Value);


		static void RegisterComponentMutex(ReadWriteMutexGenerator& Generator)
		{
			static std::array<RWUniqueTypeID, sizeof...(ComponentT)> temp_buffer = {
				RWUniqueTypeID::Create<ComponentT>()...
			};

			Generator.RegisterComponentMutex(std::span(temp_buffer));
		}

		virtual std::span<UniqueTypeID const> GetArchetypeIndex() const override
		{
			static std::array<UniqueTypeID, sizeof...(ComponentT)> temp_buffer = {
				UniqueTypeID::Create<ComponentT>()...
			};
			return std::span(temp_buffer);
		}

		ComponentFilter(ComponentFilter const&) = default;
		ComponentFilter(ComponentFilter&&) = default;
		ComponentFilter() = default;

	protected:

		ComponentFilter(Potato::IR::MemoryResourceRecord record) : ComponentFilterInterface(record.GetResource()), record(record) {}

		Potato::IR::MemoryResourceRecord record;

		virtual void AddFilterRef() const { return DefaultIntrusiveInterface::AddRef(); }
		virtual void SubFilterRef() const { return DefaultIntrusiveInterface::SubRef(); }
		virtual void Release() override
		{
			auto re = record;
			this->~ComponentFilter();
			re.Deallocate();
		}

		friend struct Context;
	};

	/*
	template<typename AppendData, typename Func>
	struct DynamicSystemHolder : public SystemHolder
	{
		AppendData append_data;
		std::conditional_t<
			std::is_function_v<Func>,
			Func*,
			Func
		> fun;

		Potato::IR::MemoryResourceRecord record;

		DynamicSystemHolder(AppendData&& append_data, Func&& fun, std::u8string_view display_name, Property in_property, Potato::IR::MemoryResourceRecord record)
			: SystemHolder(display_name, in_property), append_data(std::move(append_data)), fun(std::move(fun)), record(record)
		{}

		virtual void SystemExecute(ExecuteContext& context) override { fun(context, append_data); }

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
		Property property,
		std::u8string_view display_prefix,
		std::pmr::memory_resource* resource
	)
		-> Ptr
	{
		using Type = DynamicSystemHolder<std::remove_cvref_t<AppendData>, std::remove_cvref_t<Func>>;

		if (resource != nullptr)
		{
			auto layout = Potato::IR::Layout::Get<Type>();
			std::size_t dis_size = SystemHolder::FormatDisplayNameSize(display_prefix, property);
			std::size_t offset = 0;
			if(dis_size != 0)
			{
				offset = Potato::IR::InsertLayoutCPP(layout, Potato::IR::Layout::GetArray<char8_t>(dis_size));
				Potato::IR::FixLayoutCPP(layout);
			}

			auto re = Potato::IR::MemoryResourceRecord::Allocate(resource, layout);

			if (re)
			{
				auto str_offset = std::span(re.GetByte(), layout.Size).subspan(offset);
				Type* ptr = new (re.Get()) Type(
					std::forward<AppendData>(append_data),
					std::forward<Func>(func),
					str_offset,
					display_prefix,
					property,
					resource
				);
				return Ptr{ ptr };
			}
		}
		return {};
	}
	*/

	template<typename ParT>
	concept DetectSystemHasGenerateFilter = requires(ParT)
	{
		{ ParT::GenerateFilter(std::declval<ReadWriteMutexGenerator&>()) };
		!std::is_same_v<decltype(ParT::GenerateFilter(std::declval<ReadWriteMutexGenerator&>())), void>;
	};

	struct SystemAutomatic
	{

		struct Holder {};

		template<typename ParT>
		struct ExtractAppendDataForParameter
		{
			using Type = std::conditional_t<std::is_same_v<ParT, ExecuteContext&>, Holder, ParT>;
			static Type Generate(ReadWriteMutexGenerator& Generator) { return {}; }
			template<typename ParT2>
				requires(std::is_same_v<Type, Holder>)
			static decltype(auto) Translate(ExecuteContext& context, ParT2&& par2) { return context; }
			template<typename ParT2>
				requires(!std::is_same_v<Type, Holder>)
			static decltype(auto) Translate(ExecuteContext& context, ParT2&& par2) { return par2; }
		};


		template<DetectSystemHasGenerateFilter ParT>
		struct ExtractAppendDataForParameter<ParT>
		{
			using Type = decltype(ParT::GenerateFilter(std::declval<ExecuteContext&>()));
			static Type Generate(ExecuteContext& Generator) { return ParT::GenerateFilter(Generator); }
			template<typename ParT2>
			static decltype(auto) Translate(ExecuteContext& context, ParT2&& par2) { return par2; }
		};

		template<typename ...ParT>
		struct ExtractAppendDataForParameters
		{

			using Type = std::tuple<
				typename ExtractAppendDataForParameter<std::remove_cvref_t<ParT>>::Type...
			>;

			static Type Construct(ReadWriteMutexGenerator& generator)
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

			static AppendDataT Generate(ReadWriteMutexGenerator& generator) { return Extract::Construct(generator); }
			static auto Execute(ExecuteContext& context, AppendDataT& append_data, Func& func)
			{
				return std::apply([&](auto&& ...par)
					{
						func(
							ExtractAppendDataForParameter<std::remove_cvref_t<decltype(par)>>::Translate(context, par)...
						);
					}, append_data);
			}
		};

		/*
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
		*/
	};

	template<typename Func>
	struct DynamicAutoSystemHolder : public SystemHolder//, public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Automatic = SystemAutomatic::ExtractTickSystem<Func>;

		typename Automatic::AppendDataT append_data;

		std::conditional_t<
			std::is_function_v<Func>,
			Func*,
			Func
		> fun;

		Potato::IR::MemoryResourceRecord record;

		DynamicAutoSystemHolder(Automatic::AppendDataT&& append_data, Func&& fun, std::u8string_view display_name, Property in_property, Potato::IR::MemoryResourceRecord record)
			: SystemHolder(in_property, display_name), append_data(std::move(append_data)), fun(std::move(fun)), record(record)
		{}

		virtual void SystemExecute(ExecuteContext& context) override
		{
			Automatic::Execute(context, append_data, fun);
		}

		virtual void Release() override
		{
			auto re = record;
			this->~DynamicAutoSystemHolder();
			record.Deallocate();
		}

		//virtual void AddTaskFlowNodeRef() const override { DefaultIntrusiveInterface::AddRef(); }
		//virtual void SubTaskFlowNodeRef() const override { DefaultIntrusiveInterface::SubRef(); }
	};

	template<typename Func, typename AppendData>
	auto SystemHolder::CreateAuto(
		Func&& func,
		AppendData&& append_data,
		Property property,
		std::u8string_view display_prefix,
		std::pmr::memory_resource* resource
	)
		-> Ptr
	{
		using Type = DynamicAutoSystemHolder<std::remove_cvref_t<Func>>;

		auto layout = Potato::IR::Layout::Get<Type>();
		std::size_t dis_size = SystemHolder::FormatDisplayNameSize(display_prefix, property);
		std::size_t offset = 0;
		if (dis_size != 0)
		{
			offset = Potato::IR::InsertLayoutCPP(layout, Potato::IR::Layout::GetArray<char8_t>(dis_size));
			Potato::IR::FixLayoutCPP(layout);
		}

		auto re = Potato::IR::MemoryResourceRecord::Allocate(resource, layout);

		if (re)
		{
			std::u8string_view dis;
			if(dis_size != 0)
			{
				auto str = std::span(re.GetByte(), re.layout.Size).subspan(offset);
				auto re2 = SystemHolder::FormatDisplayName(
					std::span(reinterpret_cast<char8_t*>(str.data()), str.size() / sizeof(char8_t)),
					display_prefix,
					property
				);
				if(re2)
				{
					std::tie(dis, property) = *re2;
				}
			}
			Type* ptr = new (re.Get()) Type(
				std::forward<AppendData>(append_data),
				std::forward<Func>(func),
				dis,
				property,
				re
			);
			return Ptr{ ptr };
		}
		return {};
	}

	export template<typename Func>
	bool Context::CreateTickSystemAuto(std::int32_t layer, Priority priority, Property property,
		Func&& func, OrderFunction order_func, Potato::Task::TaskProperty task_property, std::pmr::memory_resource* temporary_resource)
	{
		using Type = SystemAutomatic::ExtractTickSystem<Func>;
		std::pmr::monotonic_buffer_resource temp_resource(temporary_resource);
		ReadWriteMutexGenerator generator(&temp_resource);
		auto append = Type::Generate(generator);
		auto ptr = SystemHolder::CreateAuto(std::forward<Func>(func), std::move(append), property, name, &system_resource);
		return RegisterSystem(std::move(ptr), layer, priority, property, order_func, task_property, generator);
	}

	/*
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
	*/




	/*
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

	using Priority = Potato::Task::Priority;
	using Category = Potato::Task::Category;

	struct ContextProperty
	{
		Priority priority = Priority::Normal;
		Category category = Category::GLOBAL_TASK;
		std::size_t group_id = 0;
		std::thread::id thread_id;
	};

	struct Context : protected Potato::Task::Task, public Potato::Pointer::DefaultIntrusiveInterface
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
	*/
	
}
