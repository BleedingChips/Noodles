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
				std::is_lvalue_reference_v<Type>,
				UniqueTypeID::Create<std::remove_cvref_t<Type>>()
			};
		}
	};

	struct Priority
	{
		std::int32_t layout = 0;
		std::int32_t primary = 0;
		std::int32_t second = 0;
		std::strong_ordering operator<=>(Priority const&) const = default;
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
		std::span<RWUniqueTypeID const> components;
		std::span<RWUniqueTypeID const> singleton;
		std::optional<RWUniqueTypeID> system;

		bool IsConflict(ReadWriteMutex const& mutex) const;
	};

	export struct Context;

	struct ReadWriteMutexGenerator
	{

		void RegisterComponentMutex(std::span<RWUniqueTypeID const> ifs);
		void RegisterSingletonMutex(std::span<RWUniqueTypeID const> ifs);
		void SetSystemRWUniqueID();
		std::tuple<std::size_t, std::size_t> CalculateUniqueIDCount() const;
		ReadWriteMutex GetMutex() const;

	protected:

		ReadWriteMutexGenerator(std::pmr::memory_resource* template_resource) : unique_ids(template_resource){ }

		std::pmr::vector<RWUniqueTypeID> unique_ids;
		std::size_t component_count = 0;
		std::optional<RWUniqueTypeID> system_id;

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

		template<typename Func>
		static auto CreateAuto(
			Func&& func,
			ReadWriteMutexGenerator& generator,
			Property property,
			std::u8string_view display_prefix,
			std::pmr::memory_resource* resource,
			std::pmr::memory_resource* parameter_resource
		)
			-> Ptr;

		

		std::u8string_view GetDisplayName() const { return display_name; }
		Property GetProperty() const { return property; };

		static std::size_t FormatDisplayNameSize(std::u8string_view prefix, Property property);
		static std::optional<std::tuple<std::u8string_view, Property>> FormatDisplayName(std::span<char8_t> output, std::u8string_view prefix, Property property);

	protected:

		SystemHolder(Property property, std::u8string_view display_name)
			: property(property), display_name(display_name) {}

		virtual void TaskFlowNodeExecute(Potato::Task::TaskFlowStatus& status) override final;
		virtual void SystemExecute(ExecuteContext& context) = 0;

		virtual void SystemInit(Context& context) {}
		virtual void SystemRelease(Context& context) {}

		operator Potato::Task::TaskFlowNode::Ptr() { return this; }

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
		bool CreateTickSystemAuto(Priority priority, Property property,
			Func&& func, OrderFunction order_func = nullptr, Potato::Task::TaskProperty task_property = {}, 
				std::pmr::memory_resource* system_resource = std::pmr::get_default_resource(),
				std::pmr::memory_resource* parameter_resource = std::pmr::get_default_resource(),
				std::pmr::memory_resource* temporary_resource = std::pmr::get_default_resource()
		);

		bool RegisterFilter(ComponentFilterInterface::Ptr interface, SystemHolder& owner)
		{
			return manager.RegisterFilter(std::move(interface), reinterpret_cast<std::size_t>(&owner));
		}

		bool UnRegisterFilter(SystemHolder& owner)
		{
			return manager.ReleaseFilter(reinterpret_cast<std::size_t>(&owner));
		}

		void FlushStats();

	protected:

		bool RegisterSystem(SystemHolder::Ptr, Priority priority, Property property, OrderFunction func, Potato::Task::TaskProperty task_property, ReadWriteMutexGenerator& generator);
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
		std::chrono::steady_clock::time_point start_up_tick_lock;
		ArchetypeComponentManager manager;

		std::mutex system_mutex;
		struct SystemTuple
		{
			SystemHolder::Ptr system;
			Property property;
			Priority priority;
			Potato::Misc::IndexSpan<> component_index;
			Potato::Misc::IndexSpan<> singleton_index;
			std::optional<RWUniqueTypeID> system_infos;
			OrderFunction order_function;
		};

		std::pmr::vector<SystemTuple> systems;
		std::pmr::vector<RWUniqueTypeID> rw_unique_id;

		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
		friend struct SystemHolder;
	};

	template<typename ...ComponentT>
	struct ComponentFilter : protected ComponentFilterInterface
	{

		static_assert(!Potato::TMP::IsRepeat<ComponentT...>::Value);

		virtual std::span<UniqueTypeID const> GetArchetypeIndex() const override
		{
			static std::array<UniqueTypeID, sizeof...(ComponentT)> temp_buffer = {
				UniqueTypeID::Create<ComponentT>()...
			};
			return std::span(temp_buffer);
		}

		void ParameterInit(SystemHolder& owner, Context& context)
		{
			context.RegisterFilter(this, owner);
		}
		
		ComponentFilter(ReadWriteMutexGenerator& Generator, std::pmr::memory_resource* resource)
		{
			static std::array<RWUniqueTypeID, sizeof...(ComponentT)> temp_buffer = {
				RWUniqueTypeID::Create<ComponentT>()...
			};

			Generator.RegisterComponentMutex(std::span(temp_buffer));
		}
	protected:

		virtual void AddFilterRef() const override { }
		virtual void SubFilterRef() const override { }

		friend struct Context;
	};

	template<typename Type>
	concept IsExecuteContext = std::is_same_v<std::remove_cvref_t<Type>, ExecuteContext>;

	template<typename Type>
	concept EnableParameterInit = requires(Type type)
	{
		{ type.ParameterInit(std::declval<SystemHolder&>(), std::declval<Context&>()) };
	};

	template<typename Type>
	concept EnableParameterRelease = requires(Type type)
	{
		{ type.ParameterRelease(std::declval<SystemHolder&>(), std::declval<Context&>()) };
	};

	

	
	struct SystemAutomatic
	{

		template<typename Type>
		struct ParameterHolder
		{
			using RealType = std::conditional_t<
				std::is_same_v<std::remove_cvref_t<Type>, ExecuteContext>,
				Potato::TMP::ItSelf<void>,
				std::remove_cvref_t<Type>
			>;

			RealType data;

			ParameterHolder(ReadWriteMutexGenerator& Generator, std::pmr::memory_resource* system_resource)
				requires(std::is_constructible_v<RealType, ReadWriteMutexGenerator&, std::pmr::memory_resource*>)
			: data(Generator, system_resource) {}

			ParameterHolder(ReadWriteMutexGenerator& Generator, std::pmr::memory_resource* system_resource)
				requires(!std::is_constructible_v<RealType, ReadWriteMutexGenerator&, std::pmr::memory_resource*>)
			{}

			void ParameterInit(SystemHolder& owner, Context& context)
			{
				if constexpr (EnableParameterInit<RealType>)
				{
					data.ParameterInit(owner, context);
				}
			}

			void ParameterRelease(SystemHolder& owner, Context& context)
			{
				if constexpr (EnableParameterRelease<RealType>)
				{
					data.ParameterRelease(owner, context);
				}
			}

			decltype(auto) Translate(ExecuteContext& context)
			{
				if constexpr (IsExecuteContext<Type>)
					return context;
				else
					return std::ref(data);
			}
		};

		template<typename ...AT>
		struct ParameterHolders;

		template<typename Cur, typename ...AT>
		struct ParameterHolders<Cur, AT...>
		{
			ParameterHolder<Cur> cur_holder;
			ParameterHolders<AT...> other_holders;

			ParameterHolders(ReadWriteMutexGenerator& generator, std::pmr::memory_resource* resource)
				: cur_holder(generator, resource), other_holders(generator, resource)
			{
			}

			decltype(auto) Get(std::integral_constant<std::size_t, 0>, ExecuteContext& context) { return cur_holder.Translate(context);  }
			template<std::size_t i>
			decltype(auto) Get(std::integral_constant<std::size_t, i>, ExecuteContext& context) { return other_holders.Get(std::integral_constant<std::size_t, i - 1>{}, context); }

			void ParameterInit(SystemHolder& owner, Context& context)
			{
				cur_holder.ParameterInit(owner, context);
				other_holders.ParameterInit(owner, context);
			}

			void ParameterRelease(SystemHolder& owner, Context& context)
			{
				other_holders.ParameterRelease(owner, context);
				cur_holder.ParameterInit(owner, context);
			}
		};

		template<>
		struct ParameterHolders<>
		{
			ParameterHolders(ReadWriteMutexGenerator& generator, std::pmr::memory_resource* resource){}

			void ParameterInit(SystemHolder& owner, Context& context) {}
			void ParameterRelease(SystemHolder& owner, Context& context){}
		};

		template<typename ...ParT>
		struct ExtractAppendDataForParameters
		{
			using Type = ParameterHolders<ParT...>;
			using Index = std::make_index_sequence<sizeof...(ParT)>;
		};

		template<typename Func>
		struct ExtractTickSystem
		{
			using Extract = typename Potato::TMP::FunctionInfo<std::remove_cvref_t<Func>>::template PackParameters<ExtractAppendDataForParameters>;

			using AppendDataT = typename Extract::Type;

			//static AppendDataT Generate(ReadWriteMutexGenerator& generator) { return Extract::Construct(generator); }
			static auto Execute(ExecuteContext& context, AppendDataT& append_data, Func& func)
			{
				return Execute(context, append_data, func, typename Extract::Index{});
			}

			template<std::size_t ...i>
			static auto Execute(ExecuteContext& context, AppendDataT& append_data, Func& func, std::index_sequence<i...>)
			{
				return func(append_data.Get(std::integral_constant<std::size_t, i>{}, context)...);
			}
		};
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

		DynamicAutoSystemHolder(ReadWriteMutexGenerator& generator, Func&& fun, std::u8string_view display_name, Property in_property, Potato::IR::MemoryResourceRecord record, std::pmr::memory_resource* parameter_resource)
			: SystemHolder(in_property, display_name), append_data(generator, parameter_resource), fun(std::move(fun)), record(record)
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

		virtual void SystemInit(Context& context) override
		{
			append_data.ParameterInit(*this, context);
		}

		virtual void SystemRelease(Context& context) override
		{
			append_data.ParameterRelease(*this, context);
			context.UnRegisterFilter(*this);
		}
	};

	template<typename Func>
	auto SystemHolder::CreateAuto(
		Func&& func,
		ReadWriteMutexGenerator& generator,
		Property property,
		std::u8string_view display_prefix,
		std::pmr::memory_resource* resource,
		std::pmr::memory_resource* parameter_resource
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
				generator,
				std::forward<Func>(func),
				dis,
				property,
				re,
				parameter_resource
			);
			return Ptr{ ptr };
		}
		return {};
	}

	export template<typename Func>
	bool Context::CreateTickSystemAuto(Priority priority, Property property,
		Func&& func, OrderFunction order_func, Potato::Task::TaskProperty task_property, 
		std::pmr::memory_resource* system_resource,
		std::pmr::memory_resource* parameter_resource,
		std::pmr::memory_resource* temporary_resource
	)
	{
		//using Type = SystemAutomatic::ExtractTickSystem<Func>;
		std::pmr::monotonic_buffer_resource temp_resource(temporary_resource);
		ReadWriteMutexGenerator generator(&temp_resource);
		auto ptr = SystemHolder::CreateAuto(std::forward<Func>(func), generator, property, name, system_resource, parameter_resource);
		return RegisterSystem(std::move(ptr), priority, property, order_func, task_property, generator);
	}
	
}
