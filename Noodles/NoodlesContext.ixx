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
		UniqueTypeID type_id;


		template<AcceptableFilterType Type>
		static RWUniqueTypeID Create()
		{
			return RWUniqueTypeID{
				IsFilterWriteType<Type>,
				IgnoreMutexComponentType<Type>,
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
		//std::span<RWUniqueTypeID const> outside;

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

		bool RemoveEntity(Entity& entity) { return manager.ReleaseEntity(entity); }

		template<typename SingletonT, typename ...OT>
		Potato::Pointer::ObserverPtr<SingletonT> CreateSingleton(OT&& ...ot) { return manager.CreateSingletonType<SingletonT>(std::forward<OT>(ot)...); }

		bool RegisterFilter(ComponentFilterInterface::Ptr interface, TaskFlow::Socket& owner) { return manager.RegisterFilter(std::move(interface), reinterpret_cast<std::size_t>(&owner)); }

		bool RegisterFilter(SingletonFilterInterface::Ptr interface, TaskFlow::Socket& owner) { return manager.RegisterFilter(std::move(interface), reinterpret_cast<std::size_t>(&owner)); }

		bool UnRegisterFilter(TaskFlow::Socket& owner) { return manager.ReleaseFilter(reinterpret_cast<std::size_t>(&owner)); }

		using ComponentWrapper = ArchetypeComponentManager::ComponentsWrapper;
		using EntityWrapper = ArchetypeComponentManager::EntityWrapper;

		ComponentWrapper IterateComponent(ComponentFilterInterface const& interface, std::size_t ite_index) const { return manager.ReadComponents(interface, ite_index); }


		EntityWrapper ReadEntity(Entity const& entity, ComponentFilterInterface const& interface) const { { return manager.ReadEntityComponents(entity, interface); } }
		std::optional<std::span<void*>> ReadEntityDirect(Entity const& entity, ComponentFilterInterface const& interface, std::span<void*> output) const { return manager.ReadEntityDirect(entity, interface, output); };

		Potato::Pointer::ObserverPtr<void> ReadSingleton(SingletonFilterInterface const& interface) { return manager.ReadSingleton(interface);  }

		void Quit();

		Context(Config config = {}, SyncResource resource = {});
		bool Commited(Potato::Task::TaskContext& context, Potato::Task::NodeProperty property) override;

	protected:

		

		//bool RegisterSystem(SystemHolder::Ptr, Priority priority, Property property, OrderFunction func, std::optional<Potato::Task::TaskFilter> task_filter, ReadWriteMutexGenerator& generator);
		
		//bool FlushSystemStatus(std::pmr::vector<Potato::Task::TaskFlow::ErrorNode>* error = nullptr);
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
		friend struct SystemHolder;
	};











	/*
	export struct SystemHolder : protected Potato::Task::TaskFlowNode, protected Potato::Pointer::DefaultIntrusiveInterface
	{

		using Ptr = Potato::Pointer::IntrusivePtr<SystemHolder, Potato::Task::TaskFlowNode::Wrapper>;

		template<typename Func>
		static auto CreateAuto(
			Func&& func,
			ReadWriteMutexGenerator& generator,
			std::pmr::memory_resource* resource,
			std::pmr::memory_resource* parameter_resource
		)
			-> Ptr;

		static std::size_t FormatDisplayNameSize(std::u8string_view prefix, Property property);
		static std::optional<std::tuple<std::u8string_view, Property>> FormatDisplayName(std::span<char8_t> output, std::u8string_view prefix, Property property);

	protected:

		SystemHolder(Property property, std::u8string_view display_name)
			: property(property), display_name(display_name) {}

		virtual void TaskFlowNodeExecute(Potato::Task::TaskFlowContext& context) override final;
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
	*/

	/*
	

	template<AcceptableFilterType ...ComponentT>
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
			: ComponentFilterInterface(resource)
		{
			static std::array<RWUniqueTypeID, sizeof...(ComponentT)> temp_buffer = {
				RWUniqueTypeID::Create<ComponentT>()...
			};

			Generator.RegisterComponentMutex(std::span(temp_buffer));
		}

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

		decltype(auto) IterateComponent(Context& context, std::size_t ite_index) const { return context.IterateComponent(*this, ite_index); }
		decltype(auto) IterateComponent(ExecuteContext& context, std::size_t ite_index) const { return IterateComponent(context.noodles_context, ite_index); }
		decltype(auto) ReadEntity(Context& context, Entity const& entity) const { return context.ReadEntity(entity, *this); }
		decltype(auto) ReadEntity(ExecuteContext& context, Entity const& entity) const { return ReadEntity(context.noodles_context, entity); }

	protected:

		virtual void AddFilterRef() const override { }
		virtual void SubFilterRef() const override { }

		friend struct Context;
	};  

	template<AcceptableFilterType ComponentT>
	struct SingletonFilter : protected SingletonFilterInterface
	{
		virtual UniqueTypeID RequireTypeID() const override
		{
			return UniqueTypeID::Create<ComponentT>();
		}

		void ParameterInit(SystemHolder& owner, Context& context)
		{
			context.RegisterFilter(this, owner);
		}

		SingletonFilter(ReadWriteMutexGenerator& Generator, std::pmr::memory_resource* resource)
		{
			RWUniqueTypeID tem_id = RWUniqueTypeID::Create<ComponentT>();
			Generator.RegisterSingletonMutex(std::span(&tem_id, 1));
		}

		decltype(auto) Get(Context& context) const { return Potato::Pointer::ObserverPtr<ComponentT>{ static_cast<ComponentT*>(context.ReadSingleton(*this).GetPointer()) }; }
		decltype(auto) Get(ExecuteContext& context) const { return Get(context.noodles_context); }

	protected:

		virtual void AddFilterRef() const override { }
		virtual void SubFilterRef() const override { }

		friend struct Context;
	};

	export struct ExecuteContext
	{
		Potato::Task::TaskContext& task_context;
		Context& noodles_context;
	};
	*/

	template<typename Type>
	concept IsExecuteContext = std::is_same_v<std::remove_cvref_t<Type>, ExecuteContext>;

	template<typename Type>
	concept EnableParameterInit = requires(Type type)
	{
		{ type.ParameterInit(std::declval<SystemNode&>(), std::declval<Context&>()) };
	};

	template<typename Type>
	concept EnableParameterRelease = requires(Type type)
	{
		{ type.ParameterRelease(std::declval<SystemNode&>(), std::declval<Context&>()) };
	};

	/*
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

			void ParameterInit(SystemNode& owner, Context& context)
			{
				if constexpr (EnableParameterInit<RealType>)
				{
					data.ParameterInit(owner, context);
				}
			}

			void ParameterRelease(SystemNode& owner, Context& context)
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

			void ParameterInit(SystemNode& owner, Context& context)
			{
				cur_holder.ParameterInit(owner, context);
				other_holders.ParameterInit(owner, context);
			}

			void ParameterRelease(SystemNode& owner, Context& context)
			{
				other_holders.ParameterRelease(owner, context);
				cur_holder.ParameterInit(owner, context);
			}
		};

		template<>
		struct ParameterHolders<>
		{
			ParameterHolders(ReadWriteMutexGenerator& generator, std::pmr::memory_resource* resource){}

			void ParameterInit(SystemNode& owner, Context& context) {}
			void ParameterRelease(SystemNode& owner, Context& context){}
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
				return func(append_data.Get(std::integral_constant<std::size_t, i>{}, context)...);
			}
		};
	};


	template<typename Func>
	struct DynamicAutoSystemHolder : public SystemNode
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
		Func&& func, OrderFunction order_func, std::optional<Potato::Task::TaskFilter> task_filter, 
		std::pmr::memory_resource* parameter_resource
	)
	{
		std::pmr::monotonic_buffer_resource temp_resource(temporary_resource);
		ReadWriteMutexGenerator generator(&temp_resource);
		auto ptr = SystemHolder::CreateAuto(std::forward<Func>(func), generator, property, name, system_resource, parameter_resource);
		return RegisterSystem(std::move(ptr), priority, property, order_func, task_filter, generator);
	}
	*/
	
}
