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
import PotatoGraph;

import NoodlesMisc;
import NoodlesArchetype;
import NoodlesComponent;
import NoodlesSingleton;
import NoodlesEntity;


export namespace Noodles
{
	
	template<typename Type>
	concept IsFilterWriteType = std::is_same_v<Type, std::remove_cvref_t<Type>>;

	template<typename Type>
	concept IsFilterReadType = std::is_same_v<Type, std::add_const_t<std::remove_cvref_t<Type>>>;

	template<typename Type>
	concept AcceptableFilterType = IsFilterWriteType<Type> || IsFilterReadType<Type>;

	struct Priority
	{
		std::int32_t layout = 0;
		std::int32_t primary = 0;
		std::int32_t second = 0;
		std::strong_ordering operator<=>(Priority const&) const = default;
		bool operator==(const Priority&) const = default;
	};

	struct SystemName
	{
		std::u8string_view name;
		std::u8string_view group;
		bool operator==(const SystemName& i1) const { return name == i1.name && group == i1.group; }
		Potato::IR::Layout GetSerializeLayout() const;
		void SerializeTo(std::span<std::byte> output) const;
		SystemName ReMap(std::span<std::byte> input) const;
	};

	export struct Context;
	export struct LayerTaskFlow;
	export struct ParallelExecutor;
	export struct ContextWrapper;

	struct SystemNode : protected Potato::Task::TaskFlowNode
	{

		struct Wrapper
		{
			void AddRef(SystemNode const* ptr) { ptr->AddSystemNodeRef(); }
			void SubRef(SystemNode const* ptr) { ptr->SubSystemNodeRef(); }
		};

		using Ptr = Potato::Pointer::IntrusivePtr<SystemNode, Wrapper>;

		virtual SystemName GetDisplayName() const = 0;

		struct Mutex
		{
			std::span<MarkElement const> component_mark;
			std::span<MarkElement const> component_write_mark;
			std::span<MarkElement const> singleton_mark;
			std::span<MarkElement const> singleton_write_mark;
			std::span<MarkElement const> thread_order_mark;
			std::span<MarkElement const> thread_order_write_mark;
		};

		virtual Mutex GetMutex() const = 0;
		virtual bool IsComponentOverlapping(SystemNode const& target_node) const = 0;
		virtual bool IsComponentOverlapping(ComponentFilter const& target_component_filter) = 0;

	protected:

		virtual void TaskFlowNodeExecute(Potato::Task::TaskFlowContext& status) override final;

		virtual void AddTaskFlowNodeRef() const override { AddSystemNodeRef(); }
		virtual void SubTaskFlowNodeRef() const override { SubSystemNodeRef(); }
		virtual void SystemNodeExecute(ContextWrapper& wrapper) = 0;


		virtual void AddSystemNodeRef() const = 0;
		virtual void SubSystemNodeRef() const = 0;

		friend struct Context;
		friend struct LayerTaskFlow;
		friend struct ParallelExecutor;
	};

	enum class Order
	{
		MUTEX,
		SMALLER,
		BIGGER,
		UNDEFINE
	};

	using OrderFunction = Order(*)(SystemName p1, SystemName p2);

	struct SystemNodeProperty
	{
		Priority priority;
		OrderFunction order_function = nullptr;
		Potato::Task::TaskFilter filter;
	};


	export struct LayerTaskFlow : protected Potato::Task::TaskFlow, protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<LayerTaskFlow, Potato::Task::TaskFlow::Wrapper>;

		bool AddTemporaryNodeImmediately(SystemNode::Ptr node, Potato::Task::TaskFilter filter);
		bool AddTemporaryNodeDefer(SystemNode::Ptr node, Potato::Task::TaskFilter filter);

	protected:

		bool AddTickedNode(SystemNode::Ptr node, SystemNodeProperty property);
		virtual bool Update_AssumedLocked(std::pmr::memory_resource* resource) override;
		bool AddTemporaryNodeImmediately_AssumedLocked(SystemNode::Ptr node, Potato::Task::TaskFilter filter);

		LayerTaskFlow(Potato::IR::MemoryResourceRecord record, std::int32_t layout)
			: MemoryResourceRecordIntrusiveInterface(record), layout(layout),
				preprocess_system_infos(record.GetMemoryResource())
		{
		}

		virtual void AddTaskFlowRef() const override { MemoryResourceRecordIntrusiveInterface::AddRef(); }
		virtual void SubTaskFlowRef() const override { MemoryResourceRecordIntrusiveInterface::SubRef(); }

		std::int32_t layout;

		Potato::Graph::DirectedAcyclicGraph pre_test_graph;
		
		struct SystemNodeInfo
		{
			SystemNode::Ptr node;
			Priority priority;
			OrderFunction order_func = nullptr;
			SystemName name;
			Potato::Graph::GraphNode process_graph_node;
			Potato::Graph::GraphNode pre_test_graph_node;
		};

		struct EdgeProperty
		{
			Potato::Graph::GraphNode pre_process_from;
			Potato::Graph::GraphNode pre_process_to;
			bool component_overlapping = false;
			bool singleton_overlapping = false;
		};

		std::pmr::vector<SystemNodeInfo> preprocess_system_infos;
		std::pmr::vector<EdgeProperty> edge_property;

		struct DeferInfo
		{
			SystemNode::Ptr ptr;
			Potato::Task::TaskFilter filter;
		};

		std::pmr::vector<DeferInfo> defer_temporary_system_node;

		friend struct TaskFlow::Wrapper;
		friend struct Context;
		friend struct SystemNode;
	};

	struct SystemTemplate
	{
		struct Wrapper
		{
			void AddRef(SystemTemplate const* ptr) { ptr->AddSystemTemplateRef(); }
			void SubRef(SystemTemplate const* ptr) { ptr->SubSystemTemplateRef(); }
		};

		using Ptr = Potato::Pointer::IntrusivePtr<SystemTemplate, Wrapper>;
		virtual SystemNode::Ptr CreateSystemNode(Context& context, SystemName name, std::pmr::memory_resource* resource = std::pmr::get_default_resource()) const = 0;
	protected:
		virtual void AddSystemTemplateRef() const = 0;
		virtual void SubSystemTemplateRef() const = 0;
	};

	export struct Context : protected Potato::Task::TaskFlow
	{
		struct Config
		{
			std::chrono::milliseconds min_frame_time = std::chrono::milliseconds{ 13 };
			std::size_t max_component_count = 256;
			std::size_t max_archetype_count = 128;
			std::size_t max_singleton_count = 64;
		};

		using Ptr = Potato::Pointer::IntrusivePtr<Context, TaskFlow::Wrapper>;

		Entity::Ptr CreateEntity() { return entity_manager.CreateEntity(component_manager); }

		template<typename CurType, typename ...Type>
		Entity::Ptr CreateEntity(CurType&& ctype, Type&& ...type)
		{
			auto ent = CreateEntity();
			if (ent)
			{
				if (this->AddEntityComponent(*ent, std::forward<Type>(type)...))
				{
					return ent;
				}
				else
				{
					RemoveEntity(ent);
				}
			}
			return {};
		}

		template<typename Type>
		bool AddEntityComponent(Entity& entity, Type&& type) { return entity_manager.AddEntityComponent(entity, std::forward<Type>(type)); }

		template<typename Type, typename ...OtherType>
		bool AddEntityComponent(Entity& entity, Type&& c_type, OtherType&& ...other)
		{
			if (this->AddEntityComponent(entity, std::forward<Type>(c_type)))
			{
				return this->AddEntityComponent(entity, std::forward<OtherType>(other)...);
			}
			return false;
		}

		//bool AddSystem(SystemNode::Ptr system, SystemNodeProperty property);
		//bool RemoveSystem(TaskFlow::Node& node);
		bool RemoveSystemDefer(SystemName display_name);
		bool RemoveSystemDeferByGroup(std::u8string_view group_name);

		template<typename Func>
		static SystemTemplate::Ptr CreateAutomaticSystemTemplate(Func&& func, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
			requires(std::is_copy_constructible_v<Func>);

		template<typename Func>
		SystemNode::Ptr CreateAutomaticSystem(Func&& func, SystemName name, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		SystemNode::Ptr CreateSystem(SystemTemplate const& system_template, SystemName name, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
		{
			return system_template.CreateSystemNode(*this, name, resource);
		}

		bool AddTickedSystemNode(SystemNode& node, SystemNodeProperty property);
		bool AddTemporarySystemNodeDefer(SystemNode& node, std::int32_t layout, Potato::Task::TaskFilter property);

		template<typename Function>
		bool CreateAndAddTickedAutomaticSystem(Function&& func, SystemName name, SystemNodeProperty property = {}, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
		{
			auto ptr = this->CreateAutomaticSystem(std::forward<Function>(func), name, resource);
			if (ptr)
			{
				return AddTickedSystemNode(*ptr, property);
			}
			return false;
		}

		bool RemoveEntity(Entity::Ptr entity) { return entity_manager.ReleaseEntity(std::move(entity)); }

		template<typename Type>
		bool AddSingleton(Type&& type) { return singleton_manager.AddSingleton(std::forward<Type>(type)); }


		decltype(auto) CreateComponentFilter(
			std::span<ComponentFilter::Info const> require_struct_layout,
			std::span<StructLayout::Ptr const> refuse_struct_layout,
			std::size_t identity
			)
		{
			return component_manager.CreateComponentFilter(require_struct_layout, refuse_struct_layout, identity);
		}

		decltype(auto) CreateSingletonFilter(std::span<ComponentFilter::Info const> require_struct_layout, std::size_t identity)
		{
			return singleton_manager.CreateSingletonFilter(require_struct_layout, identity);
		}

		std::optional<ComponentRowWrapper> IterateComponent_AssumedLocked(ComponentFilter const& filter, std::size_t ite_index) const
		{
			return component_manager.ReadComponentRow_AssumedLocked(filter, ite_index);
		}
		std::optional<ComponentRowWrapper> ReadEntity_AssumedLocked(Entity const& entity, ComponentFilter const& filter) const { { return entity_manager.ReadEntityComponents_AssumedLocked(component_manager, entity, filter); } }
		//decltype(auto) ReadEntityDirect_AssumedLocked(Entity const& entity, ComponentFilter const& filter, std::span<void*> output, bool prefer_modifier = true) const { return manager.ReadEntityDirect_AssumedLocked(entity, filter, output, prefer_modifier); };
		SingletonWrapper ReadSingleton_AssumedLocked(SingletonFilter const& filter) { return singleton_manager.ReadSingleton_AssumedLocked(filter); }

		void Quit();

		Context(Config config = {});
		bool Commited(Potato::Task::TaskContext& context, Potato::Task::TaskFlowNodeProperty property) override;
		std::chrono::steady_clock::duration GetFramedDuration() const { return framed_duration; }
		float GetFramedDurationInSecond() const
		{
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(GetFramedDuration());
			return ms.count() / 1000.0f;
		}

	protected:

		LayerTaskFlow* FindSubContextTaskFlow_AssumedLocked(std::int32_t layer);
		LayerTaskFlow::Ptr FindOrCreateContextTaskFlow_AssumedLocked(std::int32_t layer);

		virtual void TaskFlowExecuteBegin(Potato::Task::TaskFlowContext& context) override;
		virtual void TaskFlowExecuteEnd(Potato::Task::TaskFlowContext& context) override;
		virtual void AddContextRef() const = 0;
		virtual void SubContextRef() const = 0;
		virtual void AddTaskFlowRef() const override { AddContextRef(); }
		virtual void SubTaskFlowRef() const override { SubContextRef(); }

		std::atomic<std::chrono::steady_clock::duration> framed_duration;

		std::mutex mutex;
		Config config;
		bool require_quit = false;
		std::chrono::steady_clock::time_point start_up_tick_lock;

		ComponentManager component_manager;
		EntityManager entity_manager;
		SingletonManager singleton_manager;

		std::pmr::memory_resource* context_resource = nullptr;
		std::pmr::memory_resource* system_resource = nullptr;
		std::pmr::memory_resource* entity_resource = nullptr;
		std::pmr::memory_resource* temporary_resource = nullptr;

		friend struct TaskFlow::Wrapper;
		friend struct SystemNode;
		friend struct ParallelExecutor;
	};

	struct ParallelInfo
	{
		enum class Status
		{
			None,
			Parallel,
			Done
		};
		Status status = Status::None;
		std::size_t total_count = 0;
		std::size_t current_index = 0;
		std::size_t user_index = 0;
	};

	export struct ContextWrapper
	{
		ContextWrapper(
			Potato::Task::TaskContext& task_context,
			Potato::Task::TaskFlowNodeProperty node_property,
			Context& noodles_context,
			LayerTaskFlow& current_layout_flow,
			SystemNode& current_node,
			std::size_t node_index,
			ParallelInfo parallel_info
		)
			: task_context(task_context), node_property(node_property),
			noodles_context(noodles_context), current_layout_flow(current_layout_flow),
			current_node(current_node), node_index(node_index),
			parallel_info(parallel_info)
		{
		}
		Potato::Task::TaskContext& GetTaskContext() const { return task_context; }
		ParallelInfo GetParrallelInfo() const { return parallel_info; }
		bool CommitParallelTask(std::size_t user_index, std::size_t total_count, std::size_t executor_count, std::pmr::memory_resource* resource = std::pmr::get_default_resource());
		Potato::Task::TaskFlowNodeProperty GetProperty() const { return node_property; }
		Context& GetContext() const { return noodles_context; }
		template<typename Func>
		SystemNode::Ptr CreateAutomaticSystem(Func&& func, SystemName name, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
		{
			return noodles_context.CreateAutomaticSystem(std::forward<Func>(func), name, resource);
		}
		bool AddTemporaryNodeImmediately(SystemNode& node, Potato::Task::TaskFilter filter) { return current_layout_flow.AddTemporaryNodeDefer(&node, std::move(filter)); }
		bool AddTemporaryNodeDefer(SystemNode& node, Potato::Task::TaskFilter filter) { return current_layout_flow.AddTemporaryNodeDefer(&node, std::move(filter)); }
		void Quit() { return noodles_context.Quit(); }
	protected:
		Potato::Task::TaskContext& task_context;
		Potato::Task::TaskFlowNodeProperty node_property;
		Context& noodles_context;
		LayerTaskFlow& current_layout_flow;
		SystemNode& current_node;
		std::size_t node_index;
		ParallelInfo parallel_info;
	};


	template<AcceptableFilterType ...ComponentT>
	struct AtomicComponentFilter
	{
		
		static std::span<ComponentFilter::Info const> GetRequire()
		{
			static std::array<ComponentFilter::Info, sizeof...(ComponentT)> temp_buffer = {
				{
					IsFilterWriteType<ComponentT>, Potato::IR::StructLayout::GetStatic<ComponentT>()
				}...
			};
			return std::span(temp_buffer);
		}

		AtomicComponentFilter(Context& context, std::size_t identity)
		{
			filter = context.CreateComponentFilter(
				GetRequire(),
				{},
				identity
			);
			assert(filter);
		}

		AtomicComponentFilter(AtomicComponentFilter const&) = default;
		AtomicComponentFilter(AtomicComponentFilter&&) = default;

		template<std::size_t index>
		decltype(auto) AsSpan(ComponentRowWrapper const& wrapper) const
		{
			static_assert(index < sizeof...(ComponentT));
			using Type = Potato::TMP::FindByIndex<index, ComponentT...>::Type;
			return wrapper.AsSpan<Type>(index);
		}

		template<typename Type>
		decltype(auto) AsSpan(ComponentRowWrapper const& wrapper) const
		{
			static_assert(Potato::TMP::IsOneOfV<Type, ComponentT...>);
			constexpr std::size_t index = Potato::TMP::LocateByType<Type, ComponentT...>::Value;
			return wrapper.AsSpan<Type>(index);
		}

		std::optional<ComponentRowWrapper> IterateComponent_AssumedLocked(Context& context, std::size_t ite_index, std::span<std::size_t> output) const { return context.IterateComponent_AssumedLocked(*filter, ite_index); }
		std::optional<ComponentRowWrapper> ReadEntity_AssumedLocked(Context& context, Entity const& entity, std::span<std::size_t> output) const { { return context.ReadEntity_AssumedLocked(entity, *filter); } }
		//decltype(auto) ReadEntityDirect_AssumedLocked(Context& context, Entity const& entity, std::span<void*> output, bool prefer_modifier = true) const { return context.ReadEntityDirect_AssumedLocked(entity, *filter, output, prefer_modifier); };

		template<AcceptableFilterType ...RefuseComponent>
		struct WithRefuse : public AtomicComponentFilter<ComponentT...>
		{
			static std::span<StructLayout::Ptr const> GetResuse()
			{
				static std::array<StructLayout::Ptr, sizeof...(ComponentT)> temp_buffer = {
					Potato::IR::StructLayout::GetStatic<ComponentT>()...
				};
				return std::span(temp_buffer);
			}

			WithRefuse(Context& context, std::size_t identity)
			{
				AtomicComponentFilter::filter = context.CreateComponentFilter(
					AtomicComponentFilter::GetRequire(),
					GetResuse(),
					identity
				);
				assert(AtomicComponentFilter::filter);
			}
		};

	protected:

		AtomicComponentFilter() = default;

		ComponentFilter::Ptr filter;

		friend struct Context;
	};

	template<AcceptableFilterType ComponentT>
	struct AtomicSingletonFilter
	{
		static std::span<ComponentFilter::Info const> GetRequire()
		{
			static std::array<ComponentFilter::Info, sizeof...(ComponentT)> temp_buffer = {
				{
					IsFilterWriteType<ComponentT>, Potato::IR::StructLayout::GetStatic<ComponentT>()
				}...
			};
			return std::span(temp_buffer);
		}

		AtomicSingletonFilter(Context& context, std::size_t identity)
		{
			filter = context.CreateSingletonFilter(
				GetRequire(),
				identity
			);
			assert(filter);
		}

		decltype(auto) Get(Context& context) const { return static_cast<ComponentT*>(context.ReadSingleton_AssumedLocked(*filter)); }
		decltype(auto) Get(ContextWrapper& wrapper) const { return Get(wrapper.GetContext()); }

	protected:

		SingletonFilter::Ptr filter;

		friend struct Context;
	};

	template<AcceptableFilterType ...ComponentT>
	struct AtomicThreadOrder
	{
		AtomicThreadOrder(Context& context, std::size_t identity) {}
		AtomicThreadOrder(AtomicThreadOrder const&) = default;
		AtomicThreadOrder(AtomicThreadOrder&&) = default;
	};


	template<typename Type>
	concept IsContextWrapper = std::is_same_v<std::remove_cvref_t<Type>, ContextWrapper>;

	template<typename Type>
	concept IsCoverFromContextWrapper = std::is_constructible_v<std::remove_cvref_t<Type>, ContextWrapper&>;

	struct SystemAutomatic
	{

		template<typename Type>
		struct ParameterHolder
		{
			using RealType = std::conditional_t<
				IsContextWrapper<Type>,
				Potato::TMP::ItSelf<void>,
				std::conditional_t<
				IsCoverFromContextWrapper<Type>,
				std::optional<std::remove_cvref_t<Type>>,
				std::remove_cvref_t<Type>
				>
			>;

			RealType data;

			ParameterHolder(Context& context, std::size_t identity)
				requires(std::is_constructible_v<RealType, Context&, std::size_t>)
			: data(context, identity) {
			}

			ParameterHolder(Context& context, std::size_t identity)
				requires(!std::is_constructible_v<RealType, Context&, std::size_t>)
			{
			}

			decltype(auto) Translate(ContextWrapper& context)
			{
				if constexpr (IsContextWrapper<Type>)
					return context;
				else if constexpr (IsCoverFromContextWrapper<Type>)
				{
					assert(!data.has_value());
					data.emplace(context);
					return *data;
				}
				else
					return std::ref(data);
			}

			void Reset()
			{
				if constexpr (!IsContextWrapper<Type> && IsCoverFromContextWrapper<Type>)
				{
					assert(data.has_value());
					data.reset();
				}
			}
		};

		template<typename ...AT>
		struct ParameterHolders;

		template<typename Cur, typename ...AT>
		struct ParameterHolders<Cur, AT...>
		{
			ParameterHolder<Cur> cur_holder;
			ParameterHolders<AT...> other_holders;

			ParameterHolders(Context& context, std::size_t identity)
				: cur_holder(context, identity), other_holders(context, identity)
			{
			}

			decltype(auto) Get(std::integral_constant<std::size_t, 0>, ContextWrapper& context) { return cur_holder.Translate(context); }
			template<std::size_t i>
			decltype(auto) Get(std::integral_constant<std::size_t, i>, ContextWrapper& context) { return other_holders.Get(std::integral_constant<std::size_t, i - 1>{}, context); }

			void Reset()
			{
				cur_holder.Reset();
				other_holders.Reset();
			}
		};

		template<>
		struct ParameterHolders<>
		{
			ParameterHolders(Context& context, std::size_t identity) {}
			void Reset() {}
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

			static auto Execute(ContextWrapper& context, AppendDataT& append_data, Func& func)
			{
				return Execute(context, append_data, func, typename Extract::Index{});
			}

			template<std::size_t ...i>
			static auto Execute(ContextWrapper& context, AppendDataT& append_data, Func& func, std::index_sequence<i...>)
			{
				struct Scope
				{
					AppendDataT& ref;
					~Scope() { ref.Reset(); }
				}Scope{ append_data };
				return std::invoke(
					func,
					append_data.Get(std::integral_constant<std::size_t, i>{}, context)...
				);
			}
		};
	};


	template<typename Func>
	struct DynamicAutoSystemHolder : public SystemNode, public Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Automatic = SystemAutomatic::ExtractTickSystem<Func>;

		typename Automatic::AppendDataT append_data;

		std::conditional_t<
			std::is_function_v<Func>,
			Func*,
			Func
		> fun;

		SystemName display_name;

		DynamicAutoSystemHolder(Context& context, Func fun, Potato::IR::MemoryResourceRecord record, SystemName display_name)
			: append_data(context), fun(std::move(fun)), MemoryResourceRecordIntrusiveInterface(record), display_name(display_name)
		{
		}

		virtual void SystemNodeExecute(ContextWrapper& context) override
		{
			if constexpr (std::is_function_v<Func>)
			{
				Automatic::Execute(context, append_data, *fun);
			}
			else
			{
				Automatic::Execute(context, append_data, fun);
			}

		}

		void AddSystemNodeRef() const override { MemoryResourceRecordIntrusiveInterface::AddRef(); }
		void SubSystemNodeRef() const override { MemoryResourceRecordIntrusiveInterface::SubRef(); }
		virtual SystemName GetDisplayName() const override { return display_name; }
	};

	template<typename Function>
	SystemNode::Ptr Context::CreateAutomaticSystem(Function&& func, SystemName name, std::pmr::memory_resource* resource)
	{
		using Type = DynamicAutoSystemHolder<std::remove_cvref_t<Function>>;
		auto layout = Potato::MemLayout::MemLayoutCPP::Get<Type>();
		auto str_layout = name.GetSerializeLayout();
		auto offset = layout.Insert(str_layout);
		auto re = Potato::IR::MemoryResourceRecord::Allocate(resource, layout);
		if (re)
		{
			name.SerializeTo({ re.GetByte() + offset, str_layout.size });
			auto new_name = name.ReMap({ re.GetByte() + offset, str_layout.size });
			return new (re.Get()) Type{ *this, std::forward<Function>(func), re, new_name };
		}
		return {};
	}

	template<typename Func>
	struct SystemTemplateHolder : public SystemTemplate, public Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		Func func;
		virtual SystemNode::Ptr CreateSystemNode(Context& context, SystemName name, std::pmr::memory_resource* resource) const override
		{
			return context.CreateAutomaticSystem(func, name, resource);
		}
		SystemTemplateHolder(Potato::IR::MemoryResourceRecord record, Func&& func) : MemoryResourceRecordIntrusiveInterface(record), func(std::move(func)) {}
	protected:
		virtual void AddSystemTemplateRef() const override { MemoryResourceRecordIntrusiveInterface::AddRef(); }
		virtual void SubSystemTemplateRef() const override { MemoryResourceRecordIntrusiveInterface::SubRef(); }
	};

	template<typename Func>
	SystemTemplate::Ptr Context::CreateAutomaticSystemTemplate(Func&& func, std::pmr::memory_resource* resource)
		requires(std::is_copy_constructible_v<Func>)
	{
		using Type = std::remove_cvref_t<Func>;
		return Potato::IR::MemoryResourceRecord::AllocateAndConstruct<SystemTemplateHolder<Type>>(
			resource,
			std::forward<Func>(func)
		);
	}

	/*
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

	

	

	struct ReadWriteMutexIndex
	{
		Potato::Misc::IndexSpan<> components_span;
		Potato::Misc::IndexSpan<> singleton_span;
		Potato::Misc::IndexSpan<> user_modify;
	};

	struct ReadWriteMutex
	{
		std::span<RWUniqueTypeID const> total_type_id;
		ReadWriteMutexIndex index;
		bool IsConflict(ReadWriteMutex const& mutex) const;
	};

	export struct Context;
	export struct SubContextTaskFlow;
	export struct ParallelExecutor;

	struct ReadWriteMutexGenerator
	{

		void RegisterComponentMutex(std::span<RWUniqueTypeID const> ifs);
		void RegisterSingletonMutex(std::span<RWUniqueTypeID const> ifs);
		void RegisterUserModifyMutex(std::span<RWUniqueTypeID const> ifs);
		std::tuple<std::size_t, std::size_t> CalculateUniqueIDCount() const;
		ReadWriteMutex GetMutex() const;
		void RecoverModify() { unique_ids.resize(old_index); }

		ReadWriteMutexGenerator(std::pmr::vector<RWUniqueTypeID>& reference_vector) : unique_ids(reference_vector), old_index(reference_vector.size()){ }

	protected:

		std::pmr::vector<RWUniqueTypeID>& unique_ids;
		std::size_t old_index;
		std::size_t component_count = 0;
		std::size_t singleton_count = 0;
		std::size_t user_modify_count = 0;

		friend struct Context;
		friend struct SubContextTaskFlow;
	};

	export struct ContextWrapper;

	

	enum class Order
	{
		MUTEX,
		SMALLER,
		BIGGER,
		UNDEFINE
	};

	using OrderFunction = Order(*)(SystemName p1, SystemName p2);

	struct SystemNodeProperty
	{
		Priority priority;
		OrderFunction order_function = nullptr;
		Potato::Task::TaskFilter filter;
	};

	export struct SubContextTaskFlow : public Potato::Task::TaskFlow, protected Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<SubContextTaskFlow, Potato::Task::TaskFlow::Wrapper>;

		bool AddTemporaryNodeImmediately(SystemNode::Ptr node, Potato::Task::TaskFilter filter);
		bool AddTemporaryNodeDefer(SystemNode::Ptr node, Potato::Task::TaskFilter filter);
	
	protected:

		bool AddTickedNode(SystemNode::Ptr node, SystemNodeProperty property);
		virtual bool Update_AssumedLocked(std::pmr::memory_resource* resource) override;
		bool AddTemporaryNodeImmediately_AssumedLocked(SystemNode::Ptr node, Potato::Task::TaskFilter filter);

		SubContextTaskFlow(Potato::IR::MemoryResourceRecord record, std::int32_t layout)
			: record(record), layout(layout),
			preprocess_rw_id(record.GetMemoryResource()), preprocess_system_infos(record.GetMemoryResource())
			, process_rw_id(record.GetMemoryResource()), process_system_infos(record.GetMemoryResource())
		{}

		virtual void AddTaskFlowRef() const override { DefaultIntrusiveInterface::AddRef(); }
		virtual void SubTaskFlowRef() const override { DefaultIntrusiveInterface::SubRef(); }


		void Release() override{ auto re = record; this->~SubContextTaskFlow(); re.Deallocate(); }

		Potato::IR::MemoryResourceRecord record;
		std::int32_t layout;
		std::pmr::vector<RWUniqueTypeID> preprocess_rw_id;
		struct SystemNodeInfo
		{
			ReadWriteMutexIndex read_write_mutex;
			Priority priority;
			OrderFunction order_func = nullptr;
			SystemName name;
		};
		std::pmr::vector<SystemNodeInfo> preprocess_system_infos;

		struct DeferInfo
		{
			SystemNode::Ptr ptr;
			Potato::Task::TaskFilter filter;
		};

		std::pmr::vector<DeferInfo> defer_temporary_system_node;

		struct ProcessSystemInfo
		{
			ReadWriteMutexIndex read_write_mutex;
		};

		std::size_t temporary_rw_id_offset = 0;
		std::pmr::vector<RWUniqueTypeID> process_rw_id;
		std::pmr::vector<ProcessSystemInfo> process_system_infos;

		friend struct TaskFlow::Wrapper;
		friend struct Context;
		friend struct SystemNode;
	};

	
	*/
}
