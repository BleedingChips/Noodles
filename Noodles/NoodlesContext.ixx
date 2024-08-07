module;

#include <cassert>

export module NoodlesContext;

import std;

import PotatoMisc;
import PotatoPointer;
import PotatoIR;
import PotatoTaskSystem;
import PotatoTaskFlow;
import PotatoTMP;


import NoodlesArchetype;
import NoodlesComponent;


export namespace Noodles
{

	template<typename Type>
	concept IsFilterWriteType = std::is_same_v<Type, std::remove_cvref_t<Type>>;

	template<typename Type>
	concept IsFilterReadType = std::is_same_v<Type, std::add_const_t<std::remove_cvref_t<Type>>>;

	template<typename Type>
	concept AcceptableFilterType = IsFilterWriteType<Type> || IsFilterReadType<Type>;

	template<typename Type>
	concept IgnoreMutexComponentType = requires(Type)
	{
		requires(Type::NoodlesProperty::ignore_mutex);
	};



	struct RWUniqueTypeID
	{
		bool is_write = false;
		bool ignore_mutex = false;
		AtomicType::Ptr atomic_type;


		template<AcceptableFilterType Type>
		static RWUniqueTypeID Create()
		{
			return RWUniqueTypeID{
				IsFilterWriteType<Type>,
				IgnoreMutexComponentType<Type>,
				GetAtomicType<Type>()
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
		std::span<RWUniqueTypeID const> total_type_id;
		Potato::Misc::IndexSpan<> components_span;
		Potato::Misc::IndexSpan<> singleton_span;
		Potato::Misc::IndexSpan<> user_modify;

		bool IsConflict(ReadWriteMutex const& mutex) const;
	};

	export struct Context;

	struct ReadWriteMutexGenerator
	{

		void RegisterComponentMutex(std::span<RWUniqueTypeID const> ifs);
		void RegisterSingletonMutex(std::span<RWUniqueTypeID const> ifs);
		void RegisterUserModifyMutex(std::span<RWUniqueTypeID const> ifs);
		std::tuple<std::size_t, std::size_t> CalculateUniqueIDCount() const;
		ReadWriteMutex GetMutex() const;

	protected:

		ReadWriteMutexGenerator(std::pmr::memory_resource* template_resource) : unique_ids(template_resource){ }

		std::pmr::vector<RWUniqueTypeID> unique_ids;
		std::size_t component_count = 0;
		std::size_t singleton_count = 0;
		std::size_t user_modify_count = 0;

		friend struct Context;
	};

	export struct ExecuteContext;

	struct SystemNode : protected Potato::Task::TaskFlowNode
	{

		struct Wrapper
		{
			void AddRef(SystemNode const* ptr) { ptr->AddSystemNodeRef(); }
			void SubRef(SystemNode const* ptr) { ptr->SubSystemNodeRef(); }
		};

		using Ptr = Potato::Pointer::IntrusivePtr<SystemNode, Wrapper>;

	protected:

		virtual void FlushMutexGenerator(ReadWriteMutexGenerator& generator) const = 0;
		virtual void TaskFlowNodeExecute(Potato::Task::TaskFlowContext& status) override final;
		virtual void SystemNodeExecute(ExecuteContext& context) = 0;

		virtual void AddTaskFlowNodeRef() const override { AddSystemNodeRef(); }
		virtual void SubTaskFlowNodeRef() const override { SubSystemNodeRef(); }


		virtual void AddSystemNodeRef() const = 0;
		virtual void SubSystemNodeRef() const = 0;

		friend struct Context;
	};

	enum class Order
	{
		MUTEX,
		SMALLER,
		BIGGER,
		UNDEFINE
	};

	using OrderFunction = Order(*)(Property p1, Property p2);

	struct SystemNodeProperty
	{
		Priority priority;
		Property property;
		OrderFunction order_function = nullptr;
		Potato::Task::TaskFilter filter;
	};

	struct SystemNodeUserData : protected Potato::Task::TaskFlow::UserData, protected Potato::Pointer::DefaultIntrusiveInterface
	{

		using Ptr = Potato::Pointer::IntrusivePtr<SystemNodeUserData>;

		static Ptr Create(SystemNodeProperty property, ReadWriteMutex mutex, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

	protected:

		SystemNodeUserData(
			Potato::IR::MemoryResourceRecord record,
			SystemNodeProperty property,
			ReadWriteMutex mutex,
			std::u8string_view display_name
		)
			: record(record), property(std::move(property)), mutex(mutex), display_name(display_name)
		{
			
		}

		virtual void AddUserDataRef() const override { DefaultIntrusiveInterface::AddRef(); }
		virtual void SubUserDataRef() const override { DefaultIntrusiveInterface::SubRef(); }
		virtual void Release() override;
		
		Potato::IR::MemoryResourceRecord record;
		SystemNodeProperty property;
		ReadWriteMutex mutex;
		std::u8string_view display_name;
		friend struct Context;
		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
		friend struct Potato::Task::TaskFlow::UserData::Wrapper;
		//friend struct Potato::Task::TaskFlow::UserData::Ptr;
	};

	struct SubContextTaskFlow : public Potato::Task::TaskFlow, protected Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<SubContextTaskFlow, Potato::Task::TaskFlow::Wrapper>;

	protected:

		SubContextTaskFlow(Potato::IR::MemoryResourceRecord record, std::int32_t layout)
			: record(record), layout(layout) {}

		virtual void AddTaskFlowRef() const override { DefaultIntrusiveInterface::AddRef(); }
		virtual void SubTaskFlowRef() const override { DefaultIntrusiveInterface::SubRef(); }


		void Release() override{ auto re = record; this->~SubContextTaskFlow(); re.Deallocate(); }

		Potato::IR::MemoryResourceRecord record;
		std::int32_t layout;


		friend struct Context;
		friend struct SystemNode;
	};

	export struct Context : protected Potato::Task::TaskFlow
	{

		struct Config
		{
			std::chrono::milliseconds min_frame_time = std::chrono::milliseconds{ 13 };
		};

		struct SyncResource
		{
			std::pmr::memory_resource* context_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* entity_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* archetype_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* component_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* singleton_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* system_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* filter_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* temporary_resource = std::pmr::get_default_resource();
		};

		using Ptr = Potato::Pointer::IntrusivePtr<Context, TaskFlow::Wrapper>;

		EntityPtr CreateEntity() { return manager.CreateEntity(entity_resource); }

		template<typename CurType, typename ...Type>
		EntityPtr CreateEntity(CurType&& ctype, Type&& ...type)
		{
			auto ent = CreateEntity();
			if(ent)
			{
				if(AddEntityComponent(*ent, std::forward<Type>(type)...))
				{
					return ent;
				}else
				{
					RemoveEntity(*ent);
				}
			}
			return false;
		}

		template<typename Type>
		bool AddEntityComponent(Entity& entity, Type&& type) { return manager.AddEntityComponent(entity, std::forward<Type>(type)); }

		template<typename Type, typename ...OtherType>
		bool AddEntityComponent(Entity& entity, Type&& c_type, OtherType&& ...other)
		{
			if(AddEntityComponent(entity, std::forward<Type>(c_type)))
			{
				return AddEntityComponent(entity, std::forward<OtherType>(other)...);
			}
			return true;
		}

		bool AddSystem(SystemNode::Ptr system, SystemNodeProperty property);
		//bool RemoveSystem(TaskFlow::Node& node);
		bool RemoveSystemDefer(Property require_property);
		bool RemoveSystemDeferByGroup(std::u8string_view group_name);

		template<typename Func>
		bool CreateTickSystemAuto(Priority priority, Property property,
			Func&& func, OrderFunction order_func = nullptr, std::optional<Potato::Task::TaskFilter> task_filter = std::nullopt,
				std::pmr::memory_resource* parameter_resource = std::pmr::get_default_resource()
		);

		template<typename Function>
		SystemNode::Ptr CreateAutomaticSystemNode(Function&& func);

		template<typename Function>
		bool CreateAndAddAtomaticSystem(Function&& func, SystemNodeProperty property)
		{
			auto ptr = CreateAutomaticSystemNode(std::forward<Function>(func));
			if(ptr)
			{
				return AddSystem(std::move(ptr), property);
			}
			return false;
		}

		bool RemoveEntity(Entity& entity) { return manager.ReleaseEntity(entity); }
		template<typename SingletonT>
		bool MoveAndCreateSingleton(SingletonT&& type) { return manager.MoveAndCreateSingleton(std::forward<SingletonT>(type)); }
		decltype(auto) CreateComponentFilter(std::span<AtomicType::Ptr const> atomic_type) { return manager.CreateComponentFilter(atomic_type); }
		decltype(auto) CreateSingletonFilter(AtomicType const& atomic_type) { return manager.CreateSingletonFilter(atomic_type); }

		using ComponentWrapper = ArchetypeComponentManager::ComponentsWrapper;
		using EntityWrapper = ArchetypeComponentManager::EntityWrapper;

		decltype(auto) IterateComponent_AssumedLocked(ComponentFilter const& filter, std::size_t ite_index, std::span<std::size_t> output) const { return manager.ReadComponents_AssumedLocked(filter, ite_index, output); }
		decltype(auto) ReadEntity_AssumedLocked(Entity const& entity, ComponentFilter const& filter, std::span<std::size_t> output) const { { return manager.ReadEntityComponents_AssumedLocked(entity, filter, output); } }
		decltype(auto) ReadEntityDirect_AssumedLocked(Entity const& entity, ComponentFilter const& filter, std::span<void*> output, bool prefer_modifier = true) const { return manager.ReadEntityDirect_AssumedLocked(entity, filter, output, prefer_modifier); };
		decltype(auto) ReadSingleton_AssumedLocked(SingletonFilter const& filter) { return manager.ReadSingleton_AssumedLocked(filter); }
		
		void Quit();

		Context(Config config = {}, SyncResource resource = {});
		bool Commited(Potato::Task::TaskContext& context, Potato::Task::NodeProperty property) override;

	protected:

		virtual void TaskFlowExecuteBegin(Potato::Task::TaskFlowContext& context) override;
		virtual void TaskFlowExecuteEnd(Potato::Task::TaskFlowContext& context) override;
		virtual void AddContextRef() const = 0;
		virtual void SubContextRef() const = 0;
		virtual void AddTaskFlowRef() const override { AddContextRef(); }
		virtual void SubTaskFlowRef() const override { SubContextRef(); }

		std::mutex mutex;
		Config config;
		bool require_quit = false;
		std::chrono::steady_clock::time_point start_up_tick_lock;
		ArchetypeComponentManager manager;

		std::pmr::memory_resource* context_resource = nullptr;
		std::pmr::memory_resource* system_resource = nullptr;
		std::pmr::memory_resource* entity_resource = nullptr;
		std::pmr::memory_resource* temporary_resource = nullptr;
 
		friend struct TaskFlow::Wrapper;
		friend struct SystemNode;
	};

	export struct ExecuteContext
	{
		Context& noodles_context;
		std::u8string_view display_name;
	};

	
	template<AcceptableFilterType ...ComponentT>
	struct AtomicComponentFilter
	{

		static_assert(!Potato::TMP::IsRepeat<ComponentT...>::Value);

		static std::span<AtomicType::Ptr const> GetFilterAtomicType()
		{
			static std::array<AtomicType::Ptr, sizeof...(ComponentT)> temp_buffer = {
				GetAtomicType<ComponentT>()...
			};
			return std::span(temp_buffer);
		}

		void FlushMutexGenerator(ReadWriteMutexGenerator& generator) const
		{
			static std::array<RWUniqueTypeID, sizeof...(ComponentT)> temp_buffer = {
				RWUniqueTypeID::Create<ComponentT>()...
			};
			generator.RegisterComponentMutex(std::span(temp_buffer));
		}
		
		AtomicComponentFilter(Context& context)
			: filter(context.CreateComponentFilter(GetFilterAtomicType()))
		{
			assert(filter);
		}

		AtomicComponentFilter(AtomicComponentFilter const&) = default;
		AtomicComponentFilter(AtomicComponentFilter&&) = default;

		using OutputIndexT = std::array<std::size_t, sizeof...(ComponentT)>;

		template<std::size_t index>
		decltype(auto) GetByIndex(Context::ComponentWrapper range) const
		{
			static_assert(index < sizeof...(ComponentT));
			using Type = Potato::TMP::FindByIndex<index, ComponentT...>::Type;
			assert(index < range.output_archetype_locate.size());
			return range.archetype->Get((*range.archetype)[range.output_archetype_locate[index]], range.array_mount_point).Translate<Type>();
		}

		template<typename Type>
		decltype(auto) GetByType(Context::ComponentWrapper range) const
		{
			static_assert(Potato::TMP::IsOneOfV<Type, ComponentT...>);
			constexpr std::size_t index = Potato::TMP::LocateByType<Type, ComponentT...>::Value;
			assert(index < range.output_archetype_locate.size());
			return range.archetype->Get((*range.archetype)[range.output_archetype_locate[index]], range.array_mount_point).Translate<Type>();
		}

		
		template<std::size_t index>
		decltype(auto) GetByIndex(Context::EntityWrapper range) const
		{
			return GetByIndex<index>(range.wrapper).data() + range.mount_point;
		}

		template<typename Type>
		decltype(auto) GetByType(Context::EntityWrapper range) const
		{
			return GetByType<Type>(range.wrapper).data() + range.mount_point;
		}

		decltype(auto) IterateComponent_AssumedLocked(Context& context, std::size_t ite_index, std::span<std::size_t> output) const { return context.IterateComponent_AssumedLocked(*filter, ite_index, output); }
		decltype(auto) ReadEntity_AssumedLocked(Context& context, Entity const& entity, std::span<std::size_t> output) const { { return context.ReadEntity_AssumedLocked(entity, *filter, output); } }
		decltype(auto) ReadEntityDirect_AssumedLocked(Context& context, Entity const& entity, std::span<void*> output, bool prefer_modifier = true) const { return context.ReadEntityDirect_AssumedLocked(entity, *filter, output, prefer_modifier); };
		

	protected:

		ComponentFilter::VPtr filter;

		friend struct Context;
	};  

	template<AcceptableFilterType ComponentT>
	struct AtomicSingletonFilter
	{
		static AtomicType const& GetFilterAtomicType()
		{
			return *GetAtomicType<ComponentT>();
		}

		AtomicSingletonFilter(Context& context)
			: filter(context.CreateSingletonFilter(GetFilterAtomicType()))
		{
			assert(filter);
		}

		void FlushMutexGenerator(ReadWriteMutexGenerator& generator) const
		{
			static std::array<RWUniqueTypeID, 1> temp_buffer = {
				RWUniqueTypeID::Create<ComponentT>()
			};
			generator.RegisterSingletonMutex(std::span(temp_buffer));
		}

		decltype(auto) Get(Context& context) const { return static_cast<ComponentT*>(context.ReadSingleton_AssumedLocked(*filter)); }
		decltype(auto) Get(ExecuteContext& context) const { return Get(context.noodles_context); }

	protected:

		SingletonFilter::VPtr filter;

		friend struct Context;
	};

	template<typename Type>
	concept IsExecuteContext = std::is_same_v<std::remove_cvref_t<Type>, ExecuteContext>;

	template<typename Type>
	concept EnableFlushMutexGenerator = requires(Type type)
	{
		{ type.FlushMutexGenerator(std::declval<ReadWriteMutexGenerator&>()) };
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

			ParameterHolder(Context& context)
				requires(std::is_constructible_v<RealType, Context&>)
			: data(context) {}

			ParameterHolder(Context& context)
				requires(!std::is_constructible_v<RealType, Context&>)
			{}



			void FlushMutexGenerator(ReadWriteMutexGenerator& generator) const
			{
				if constexpr (EnableFlushMutexGenerator<RealType>)
				{
					data.FlushMutexGenerator(generator);
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

			ParameterHolders(Context& context)
				: cur_holder(context), other_holders(context)
			{
			}

			decltype(auto) Get(std::integral_constant<std::size_t, 0>, ExecuteContext& context) { return cur_holder.Translate(context);  }
			template<std::size_t i>
			decltype(auto) Get(std::integral_constant<std::size_t, i>, ExecuteContext& context) { return other_holders.Get(std::integral_constant<std::size_t, i - 1>{}, context); }

			void FlushMutexGenerator(ReadWriteMutexGenerator& generator) const
			{
				cur_holder.FlushMutexGenerator(generator);
				other_holders.FlushMutexGenerator(generator);
			}
		};

		template<>
		struct ParameterHolders<>
		{
			ParameterHolders(Context& context){}

			void FlushMutexGenerator(ReadWriteMutexGenerator& generator) const {}
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

			static auto Execute(ExecuteContext& context, AppendDataT& append_data, Func& func)
			{
				return Execute(context, append_data, func, typename Extract::Index{});
			}

			template<std::size_t ...i>
			static auto Execute(ExecuteContext& context, AppendDataT& append_data, Func& func, std::index_sequence<i...>)
			{
				return std::invoke(
					func,
					append_data.Get(std::integral_constant<std::size_t, i>{}, context)...
				);
			}
		};
	};


	template<typename Func>
	struct DynamicAutoSystemHolder : public SystemNode, public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Automatic = SystemAutomatic::ExtractTickSystem<Func>;

		typename Automatic::AppendDataT append_data;

		std::conditional_t<
			std::is_function_v<Func>,
			Func*,
			Func
		> fun;

		Potato::IR::MemoryResourceRecord record;

		DynamicAutoSystemHolder(Context& context, Func&& fun, Potato::IR::MemoryResourceRecord record)
			: append_data(context), fun(std::move(fun)), record(record)
		{}

		virtual void SystemNodeExecute(ExecuteContext& context) override
		{
			if constexpr (std::is_function_v<Func>)
			{
				Automatic::Execute(context, append_data, *fun);
			}else
			{
				Automatic::Execute(context, append_data, fun);
			}
			
		}

		virtual void Release() override
		{
			auto re = record;
			this->~DynamicAutoSystemHolder();
			record.Deallocate();
		}

		virtual void FlushMutexGenerator(ReadWriteMutexGenerator& generator) const override
		{
			append_data.FlushMutexGenerator(generator);
		}

		void AddSystemNodeRef() const override { DefaultIntrusiveInterface::AddRef(); }
		void SubSystemNodeRef() const override { DefaultIntrusiveInterface::SubRef(); }
	};

	template<typename Function>
	SystemNode::Ptr Context::CreateAutomaticSystemNode(Function&& func)
	{
		using Type = DynamicAutoSystemHolder<std::remove_cvref_t<Function>>;
		auto re = Potato::IR::MemoryResourceRecord::Allocate<Type>(system_resource);

		if (re)
		{
			return new (re.Get()) Type{*this, std::forward<Function>(func), re};
		}
		return {};
	}
	
}
