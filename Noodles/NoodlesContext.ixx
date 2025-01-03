module;

#include <cassert>

export module NoodlesContext;

import std;

import Potato;

import NoodlesMisc;
import NoodlesArchetype;
import NoodlesComponent;
import NoodlesSingleton;
import NoodlesEntity;


export namespace Noodles
{

	struct Priority
	{
		std::int32_t layout = 0;
		std::int32_t primary = 0;
		std::int32_t second = 0;
		std::int32_t third = 0;
		std::strong_ordering operator<=>(Priority const&) const = default;
		bool operator==(const Priority&) const = default;
	};

	export struct Context;
	export struct LayerTaskFlow;
	export struct ParallelExecutor;
	export struct ContextWrapper;

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

	struct SystemNode : public Potato::Task::Node
	{

		struct Wrapper
		{
			void AddRef(SystemNode const* ptr) { ptr->AddSystemNodeRef(); }
			void SubRef(SystemNode const* ptr) { ptr->SubSystemNodeRef(); }
		};

		using Ptr = Potato::Pointer::IntrusivePtr<SystemNode, Wrapper>;

		struct Mutex
		{
			StructLayoutMarksInfosView component_mark;
			StructLayoutMarksInfosView singleton_mark;
			StructLayoutMarksInfosView thread_order_mark;
		};

		virtual Mutex GetMutex() const { return {}; };

		enum class ComponentOverlappingState
		{
			NoUpdate,
			IsOverlapped,
			IsNotOverlapped
		};

		virtual ComponentOverlappingState IsComponentOverlapping(SystemNode const& target_node, std::span<MarkElement const> archetype_update, std::span<MarkElement const> archetype_usage_count) const { return ComponentOverlappingState::NoUpdate; };
		virtual ComponentOverlappingState IsComponentOverlapping(ComponentFilter const& target_component_filter, std::span<MarkElement const> archetype_update, std::span<MarkElement const> archetype_usage_count) const { return ComponentOverlappingState::NoUpdate; };

	protected:

		virtual void TaskExecute(Potato::Task::ContextWrapper& wrapper) override final;
		virtual void AddTaskNodeRef() const override final { AddSystemNodeRef(); }
		virtual void SubTaskNodeRef() const override final { SubSystemNodeRef(); }
		virtual void SystemNodeExecute(ContextWrapper& wrapper) = 0;


		virtual void AddSystemNodeRef() const = 0;
		virtual void SubSystemNodeRef() const = 0;

		friend struct Context;
		friend struct LayerTaskFlow;
		friend struct ParallelExecutor;
	};

	export struct ContextWrapper
	{
		Context& GetContext() const { return context; }
		void AddTemporarySystemNodeNextFrame(SystemNode& node, Potato::Task::Property property);
		void AddTemporarySystemNode(SystemNode& node, Potato::Task::Property property);
		ContextWrapper(Potato::Task::ContextWrapper& wrapper, Context& context, LayerTaskFlow& layer_flow, Potato::TaskGraphic::FlowProcessContext& flow_context)
			: wrapper(wrapper), context(context), layer_flow(layer_flow), flow_context(flow_context){}
		Potato::Task::Property& GetTaskProperty() const { return wrapper.GetTaskNodeProperty(); }
	protected:
		Potato::Task::ContextWrapper& wrapper;
		Context& context;
		LayerTaskFlow& layer_flow;
		Potato::TaskGraphic::FlowProcessContext& flow_context;
	};

	enum class Order
	{
		MUTEX,
		SMALLER,
		BIGGER,
		UNDEFINE
	};

	using OrderFunction = Order(*)(std::u8string_view p1, std::u8string_view p2);

	struct Property
	{
		Priority priority;
		Potato::Task::Property property;
		OrderFunction order_function = nullptr;
	};

	export struct LayerTaskFlow : public Potato::TaskGraphic::Flow, protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<LayerTaskFlow, Flow::Wrapper>;

		static void AddTemporaryNode(Context& context, Potato::TaskGraphic::FlowProcessContext& flow_context, SystemNode& node, Potato::Task::Property property);
		void AddTemporaryNodeNextFrame(SystemNode& node, Potato::Task::Property property);

	protected:

		bool AddTickedNode(SystemNode& node, Property property);
		virtual void TaskFlowExecuteBegin(Potato::Task::ContextWrapper& context) override;
		virtual void PostUpdateFromFlow(Potato::TaskGraphic::FlowProcessContext& context, Potato::Task::ContextWrapper& wrapper) override;
		static bool TemporaryNodeDetect(Context& context, SystemNode& node, Potato::Task::Node const& t_node, Potato::Task::Property const& property);

		LayerTaskFlow(Potato::IR::MemoryResourceRecord record, std::int32_t layout)
			: MemoryResourceRecordIntrusiveInterface(record), layout(layout)
		{
		}

		virtual void AddTaskGraphicFlowRef() const override { MemoryResourceRecordIntrusiveInterface::AddRef(); }
		virtual void SubTaskGraphicFlowRef() const override { MemoryResourceRecordIntrusiveInterface::SubRef(); }

		std::int32_t layout;

		Potato::Graph::DirectedAcyclicGraphImmediately worst_graph;
		
		struct SystemNodeInfo
		{
			SystemNode::Ptr node;
			Priority priority;
			OrderFunction order_func = nullptr;
			Potato::Graph::GraphNode task_node;
			Potato::Graph::GraphNode worst_graph_node;
			std::u8string_view system_name;
		};

		struct DynamicEdge
		{
			bool is_mutex = false;
			bool component_overlapping = false;
			bool singleton_overlapping = false;
			Potato::Graph::GraphNode pre_process_from;
			Potato::Graph::GraphNode pre_process_to;
			bool need_edge = false;
		};

		std::pmr::vector<SystemNodeInfo> system_infos;
		std::pmr::vector<DynamicEdge> dynamic_edges;

		struct DeferInfo
		{
			SystemNode::Ptr node;
			Potato::Task::Property property;
		};

		std::pmr::vector<DeferInfo> defer_temporary_system_node;

		friend struct Potato::TaskGraphic::Flow::Wrapper;
		friend struct Context;
		friend struct SystemNode;
		friend struct ContextWrapper;
		friend struct ParallelExecutor;
		friend struct LayerTaskFlowProcessContext;
	};

	struct SystemTemplate
	{
		struct Wrapper
		{
			void AddRef(SystemTemplate const* ptr) { ptr->AddSystemTemplateRef(); }
			void SubRef(SystemTemplate const* ptr) { ptr->SubSystemTemplateRef(); }
		};

		using Ptr = Potato::Pointer::IntrusivePtr<SystemTemplate, Wrapper>;
		virtual SystemNode::Ptr CreateSystemNode(Context& context, std::pmr::memory_resource* resource = std::pmr::get_default_resource()) const = 0;
	protected:
		virtual void AddSystemTemplateRef() const = 0;
		virtual void SubSystemTemplateRef() const = 0;
	};

	struct ThreadOrderFilter : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		StructLayoutMarksInfosView GetStructLayoutMarks() const { return marks; };
		using Ptr = Potato::Pointer::IntrusivePtr<ThreadOrderFilter>;
	protected:
		ThreadOrderFilter(Potato::IR::MemoryResourceRecord record, StructLayoutMarksInfos marks)
			: MemoryResourceRecordIntrusiveInterface(record), marks(marks) {}
		StructLayoutMarksInfos marks;
		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
		friend struct Context;
	};

	export struct Context : public Potato::TaskGraphic::Flow
	{
		struct Config
		{
			std::chrono::milliseconds min_frame_time = std::chrono::milliseconds{ 13 };
			std::size_t max_component_count = 128;
			std::size_t max_archetype_count = 128;
			std::size_t max_singleton_count = 128;
			std::size_t max_thread_order_count = 128;
		};

		struct Wrapper
		{
			void AddRef(Context const* ptr) { ptr->AddContextRef(); }
			void SubRef(Context const* ptr) { ptr->SubContextRef(); }
		};

		using Ptr = Potato::Pointer::IntrusivePtr<Context, Wrapper>;

		static Ptr Create(Config config = {}, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		Entity::Ptr CreateEntity() { return entity_manager.CreateEntity(component_manager); }

		template<typename CurType, typename ...Type>
		Entity::Ptr CreateEntity(CurType&& ctype, Type&& ...type)
		{
			auto ent = CreateEntity();
			if (ent)
			{
				if (this->AddEntityComponent(ent, std::forward<CurType>(ctype), std::forward<Type>(type)...))
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
		bool AddEntityComponent(Entity& entity, Type&& type) { return entity_manager.AddEntityComponent(component_manager, entity, std::forward<Type>(type)); }

		template<typename Type, typename ...OtherType>
		bool AddEntityComponent(Entity& entity, Type&& c_type, OtherType&& ...other)
		{
			if (this->AddEntityComponent(entity, std::forward<Type>(c_type)))
			{
				if (!this->AddEntityComponent(std::move(entity), std::forward<OtherType>(other)...))
				{
					entity_manager.RemoveEntityComponent<Type>(component_manager, entity);
					return false;
				}
				return true;
			}
			return false;
		}

		//bool AddSystem(SystemNode::Ptr system, SystemNodeProperty property);
		//bool RemoveSystem(TaskFlow::Node& node);
		bool RemoveSystemDefer(std::u8string_view system_name);
		//bool RemoveSystemDeferByGroup(std::u8string_view group_name);

		template<typename Func>
		static SystemTemplate::Ptr CreateAutomaticSystemTemplate(Func&& func, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
			requires(std::is_copy_constructible_v<Func>);

		template<typename Func>
		SystemNode::Ptr CreateAutomaticSystem(Func&& func, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		SystemNode::Ptr CreateSystem(SystemTemplate const& system_template, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
		{
			return system_template.CreateSystemNode(*this, resource);
		}

		bool AddTickedSystemNode(SystemNode& node, Property property);
		bool AddTemporarySystemNodeNextFrame(SystemNode& node, std::int32_t layout, Potato::Task::Property property);

		template<typename Function>
		bool CreateAndAddTickedAutomaticSystem(Function&& func, Property property = {}, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
		{
			auto ptr = this->CreateAutomaticSystem(std::forward<Function>(func), resource);
			if (ptr)
			{
				return this->AddTickedSystemNode(*ptr, std::move(property));
			}
			return false;
		}

		bool RemoveEntity(Entity::Ptr entity) { return entity_manager.ReleaseEntity(std::move(entity)); }

		template<typename Type>
		bool AddSingleton(Type&& type) { return singleton_manager.AddSingleton(std::forward<Type>(type)); }


		decltype(auto) CreateComponentFilter(
			std::span<StructLayoutWriteProperty const> require_struct_layout,
			std::span<StructLayout::Ptr const> refuse_struct_layout,
			std::size_t identity
			)
		{
			return component_manager.CreateComponentFilter(require_struct_layout, refuse_struct_layout, identity);
		}

		decltype(auto) CreateSingletonFilter(std::span<StructLayoutWriteProperty const> require_struct_layout, std::size_t identity)
		{
			return singleton_manager.CreateSingletonFilter(require_struct_layout, identity);
		}

		bool IterateComponent_AssumedLocked(ComponentFilter const& filter, std::size_t ite_index, ComponentAccessor& accessor) const { return component_manager.ReadComponentRow_AssumedLocked(filter, ite_index, accessor); }
		bool ReadEntity_AssumedLocked(Entity const& entity, ComponentFilter const& filter, ComponentAccessor& accessor) const { { return entity_manager.ReadEntityComponents_AssumedLocked(component_manager, entity, filter, accessor); } }
		//decltype(auto) ReadEntityDirect_AssumedLocked(Entity const& entity, ComponentFilter const& filter, std::span<void*> output, bool prefer_modifier = true) const { return manager.ReadEntityDirect_AssumedLocked(entity, filter, output, prefer_modifier); };
		bool ReadSingleton_AssumedLocked(SingletonFilter const& filter, SingletonAccessor& accessor) { return singleton_manager.ReadSingleton_AssumedLocked(filter, accessor); }

		void Quit();

		
		std::chrono::steady_clock::duration GetFramedDuration() const { return framed_duration; }
		float GetFramedDurationInSecond() const
		{
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(GetFramedDuration());
			return ms.count() / 1000.0f;
		}
		
		ThreadOrderFilter::Ptr CreateThreadOrderFilter(std::span<StructLayoutWriteProperty const> info, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

	protected:

		Context(Config config = {});

		LayerTaskFlow* FindSubContextTaskFlow_AssumedLocked(std::int32_t layer);
		LayerTaskFlow::Ptr FindOrCreateContextTaskFlow_AssumedLocked(std::int32_t layer);

		virtual void TaskFlowExecuteBegin(Potato::Task::ContextWrapper& context) override;
		virtual void TaskFlowExecuteEnd(Potato::Task::ContextWrapper& context) override;
		virtual void AddContextRef() const = 0;
		virtual void SubContextRef() const = 0;
		virtual void AddTaskGraphicFlowRef() const final { AddContextRef(); }
		virtual void SubTaskGraphicFlowRef() const final { SubContextRef(); }

		std::atomic<std::chrono::steady_clock::duration> framed_duration;

		StructLayoutMarkIndexManager thread_order_manager;

		std::mutex mutex;
		Config config;
		bool require_quit = false;
		std::chrono::steady_clock::time_point start_up_tick_lock;

		ComponentManager component_manager;
		EntityManager entity_manager;
		SingletonManager singleton_manager;

		friend struct Potato::TaskGraphic::Flow::Wrapper;
		friend struct SystemNode;
		friend struct ParallelExecutor;
		friend struct LayerTaskFlow;
	};


	template<AcceptableFilterType ...ComponentT>
	struct AtomicComponentFilter
	{
		
		static std::span<StructLayoutWriteProperty const> GetRequire()
		{
			static std::array<StructLayoutWriteProperty, sizeof...(ComponentT)> temp_buffer = {
				StructLayoutWriteProperty::GetComponent<ComponentT>()...
			};
			return std::span(temp_buffer);
		}

		AtomicComponentFilter(Context& context, std::size_t identity)
			: accessor(std::span(buffer))
		{
			filter = context.CreateComponentFilter(
				GetRequire(),
				{},
				identity
			);
			assert(filter);
		}

		AtomicComponentFilter(AtomicComponentFilter const& in)
			: filter(in.filter), accessor(in.accessor, std::span(buffer))
		{
			
		}
		AtomicComponentFilter(AtomicComponentFilter&&) = delete;

		template<std::size_t index>
		decltype(auto) AsSpan() const
		{
			static_assert(index < sizeof...(ComponentT));
			using Type = Potato::TMP::FindByIndex<index, ComponentT...>::Type;
			return accessor.AsSpan<Type>(index);
		}

		template<typename Type>
		decltype(auto) AsSpan() const
		{
			static_assert(Potato::TMP::IsOneOfV<Type, ComponentT...>);
			constexpr std::size_t index = Potato::TMP::LocateByType<Type, ComponentT...>::Value;
			return accessor.AsSpan<Type>(index);
		}

		bool IterateComponent(Context& context, std::size_t ite_index) { return context.IterateComponent_AssumedLocked(*filter, ite_index, accessor); }
		bool IterateComponent(ContextWrapper& context_wrapper, std::size_t ite_index) { return IterateComponent(context_wrapper.GetContext(), ite_index); }
		bool ReadEntity(Context& context, Entity const& entity) { return context.ReadEntity_AssumedLocked(entity, *filter, accessor); }
		bool ReadEntity(ContextWrapper& context_wrapper, Entity const& entity) { return ReadEntity(context_wrapper.GetContext(), entity); }
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

			WithRefuse(Context& context, std::size_t identity) : AtomicComponentFilter(context, identity) {}

			WithRefuse(WithRefuse const& in) : AtomicComponentFilter(in) {}
		};

		SystemNode::Mutex GetSystemNodeMutex() const
		{
			return {
				filter->GetRequiredStructLayoutMarks(),
				{},
				{}
			};
		}

		SystemNode::ComponentOverlappingState IsComponentOverlapping(SystemNode const& target_node, std::span<MarkElement const> archetype_update,  std::span<MarkElement const> component_usage) const
		{
			if (MarkElement::IsOverlapping(filter->GetArchetypeMarkArray(), archetype_update))
			{
				return target_node.IsComponentOverlapping(*filter, archetype_update, component_usage);
			}else
			{
				return SystemNode::ComponentOverlappingState::NoUpdate;
			}
		}

		SystemNode::ComponentOverlappingState IsComponentOverlapping(ComponentFilter const& filter, std::span<MarkElement const> archetype_update, std::span<MarkElement const> component_usage) const
		{
			if (MarkElement::IsOverlapping(this->filter->GetArchetypeMarkArray(), archetype_update))
			{
				if (MarkElement::IsOverlappingWithMask(
					this->filter->GetArchetypeMarkArray(),
					filter.GetArchetypeMarkArray(),
					component_usage
				) && this->filter->GetRequiredStructLayoutMarks().WriteConfig(filter.GetRequiredStructLayoutMarks()))
				{
					return SystemNode::ComponentOverlappingState::IsOverlapped;
				}
				return SystemNode::ComponentOverlappingState::IsNotOverlapped;
			}
			else
			{
				return SystemNode::ComponentOverlappingState::NoUpdate;
			}
		}



	protected:

		ComponentFilter::Ptr filter;
		std::array<void*, sizeof...(ComponentT)> buffer;
		ComponentAccessor accessor;

		friend struct Context;
	};

	template<AcceptableFilterType ...ComponentT>
	struct AtomicSingletonFilter
	{
		static std::span<StructLayoutWriteProperty const> GetRequire()
		{
			static std::array<StructLayoutWriteProperty, sizeof...(ComponentT)> temp_buffer = {
				StructLayoutWriteProperty::GetSingleton<ComponentT>()...
			};
			return std::span(temp_buffer);
		}

		AtomicSingletonFilter(Context& context, std::size_t identity)
			: accessor(std::span(buffers))
		{
			filter = context.CreateSingletonFilter(
				GetRequire(),
				identity
			);
			assert(filter);
		}

		AtomicSingletonFilter(AtomicSingletonFilter const& other)
			: filter(other.filter), accessor(other.accessor, std::span(buffers))
		{
			assert(filter);
		}

		decltype(auto) GetSingletons(Context& context) { return context.ReadSingleton_AssumedLocked(*filter, accessor); }
		decltype(auto) GetSingletons(ContextWrapper& context_wrapper) { return GetSingletons(context_wrapper.GetContext()); }

		template<std::size_t index> auto Get() const
		{
			using Type = typename Potato::TMP::FindByIndex<index, ComponentT...>::Type;
			return accessor.As<Type>(index);
		}

		template<typename Type> Type* Get() const requires(Potato::TMP::IsOneOfV<Type, ComponentT...>)
		{
			constexpr std::size_t index = Potato::TMP::LocateByType<Type, ComponentT...>::Value;
			return accessor.As<Type>(index);
		}

		SystemNode::Mutex GetSystemNodeMutex() const
		{
			return {
				{},
				filter->GetRequiredStructLayoutMarks(),
				{}
			};
		}

	protected:

		SingletonFilter::Ptr filter;
		std::array<void*, sizeof...(ComponentT)> buffers;
		SingletonAccessor accessor;
		friend struct Context;
	};

	template<AcceptableFilterType ...ComponentT>
	struct AtomicThreadOrder
	{
		AtomicThreadOrder(Context& context, std::size_t identity)
		{
			static std::array<StructLayoutWriteProperty, sizeof...(ComponentT)> temp_buffer = {
				StructLayoutWriteProperty::Get<ComponentT>()...
			};
			filter = context.CreateThreadOrderFilter(temp_buffer);
		}
		AtomicThreadOrder(AtomicThreadOrder const&) = default;
		AtomicThreadOrder(AtomicThreadOrder&&) = default;

		ThreadOrderFilter::Ptr filter;

		SystemNode::Mutex GetSystemNodeMutex() const
		{
			return {{}, {}, filter->GetStructLayoutMarks()};
		}
	};


	template<typename Type>
	concept IsContextWrapper = std::is_same_v<std::remove_cvref_t<Type>, ContextWrapper>;

	template<typename Type>
	concept IsCoverFromContextWrapper = std::is_constructible_v<std::remove_cvref_t<Type>, ContextWrapper&>;

	template<typename Type>
	concept HasSystemMutexFunctionWrapper = requires(Type const& type)
	{
		{type.GetSystemNodeMutex()} -> std::same_as<SystemNode::Mutex>;
	};

	template<typename Type>
	concept HasIsComponentOverlappingWithSystemNodeFunctionWrapper = requires(Type const& type)
	{
		{ type.IsComponentOverlapping(std::declval<SystemNode const&>(), std::span<MarkElement const>{}, std::span<MarkElement const>{}) } -> std::same_as<SystemNode::ComponentOverlappingState>;
	};

	template<typename Type>
	concept HasIsComponentOverlappingWithComponentFilterFunctionWrapper = requires(Type const& type)
	{
		{ type.IsComponentOverlapping(std::declval<ComponentFilter const&>(), std::span<MarkElement const>{}, std::span<MarkElement const>{}) } -> std::same_as<SystemNode::ComponentOverlappingState>;
	};

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

			SystemNode::Mutex GetSystemNodeMutex() const
			{
				if constexpr (HasSystemMutexFunctionWrapper<RealType>)
				{
					return data.GetSystemNodeMutex();
				}else
				{
					return {};
				}
			}

			void Reset()
			{
				if constexpr (!IsContextWrapper<Type> && IsCoverFromContextWrapper<Type>)
				{
					assert(data.has_value());
					data.reset();
				}
			}

			SystemNode::ComponentOverlappingState IsComponentOverlapping(SystemNode const& target_node, std::span<MarkElement const> archetype_update, std::span<MarkElement const> component_usage) const
			{
				if constexpr (HasIsComponentOverlappingWithSystemNodeFunctionWrapper<RealType>)
				{
					return data.IsComponentOverlapping(target_node, archetype_update, component_usage);
				}
				return SystemNode::ComponentOverlappingState::NoUpdate;
			}

			SystemNode::ComponentOverlappingState IsComponentOverlapping(ComponentFilter const& filter, std::span<MarkElement const> archetype_update, std::span<MarkElement const> component_usage) const
			{
				if constexpr (HasIsComponentOverlappingWithComponentFilterFunctionWrapper<RealType>)
				{
					return data.IsComponentOverlapping(filter, archetype_update, component_usage);
				}
				return SystemNode::ComponentOverlappingState::NoUpdate;
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

			void FlushSystemNodeMutex(StructLayoutMarksInfos component, StructLayoutMarksInfos singleton, StructLayoutMarksInfos thread_order) const
			{
				SystemNode::Mutex cur = cur_holder.GetSystemNodeMutex();
				if (cur.component_mark)
				{
					component.MarkFrom(cur.component_mark);
				}
				if (cur.singleton_mark)
				{
					singleton.MarkFrom(cur.singleton_mark);
				}
				if (cur.thread_order_mark)
				{
					thread_order.MarkFrom(cur.thread_order_mark);
				}
				other_holders.FlushSystemNodeMutex(component, singleton, thread_order);
			}

			SystemNode::ComponentOverlappingState IsComponentOverlapping(SystemNode::ComponentOverlappingState start_update_state, SystemNode const& target_node, std::span<MarkElement const> archetype_update, std::span<MarkElement const> component_usage) const
			{
				auto state = cur_holder.IsComponentOverlapping(target_node, archetype_update, component_usage);
				switch (state)
				{
				case SystemNode::ComponentOverlappingState::NoUpdate:
					return other_holders.IsComponentOverlapping(start_update_state, target_node, archetype_update, component_usage);
				case SystemNode::ComponentOverlappingState::IsNotOverlapped:
					return other_holders.IsComponentOverlapping(SystemNode::ComponentOverlappingState::IsNotOverlapped, target_node, archetype_update, component_usage);
				default:
					return SystemNode::ComponentOverlappingState::IsOverlapped;
				}
			}

			SystemNode::ComponentOverlappingState IsComponentOverlapping(SystemNode::ComponentOverlappingState start_update_state, ComponentFilter const& filter, std::span<MarkElement const> archetype_update, std::span<MarkElement const> component_usage) const
			{
				auto state = cur_holder.IsComponentOverlapping(filter, archetype_update, component_usage);
				switch (state)
				{
				case SystemNode::ComponentOverlappingState::NoUpdate:
					return other_holders.IsComponentOverlapping(start_update_state, filter, archetype_update, component_usage);
				case SystemNode::ComponentOverlappingState::IsNotOverlapped:
					return other_holders.IsComponentOverlapping(SystemNode::ComponentOverlappingState::IsNotOverlapped, filter, archetype_update, component_usage);
				default:
					return SystemNode::ComponentOverlappingState::IsOverlapped;
				}
			}
		};

		template<>
		struct ParameterHolders<>
		{
			ParameterHolders(Context& context, std::size_t identity) {}
			void Reset() {}
			SystemNode::ComponentOverlappingState IsComponentOverlapping(SystemNode::ComponentOverlappingState start_update_state, SystemNode const& target_node, std::span<MarkElement const> archetype_update, std::span<MarkElement const> component_usage) const { return start_update_state; }
			SystemNode::ComponentOverlappingState IsComponentOverlapping(SystemNode::ComponentOverlappingState start_update_state, ComponentFilter const& filter, std::span<MarkElement const> archetype_update, std::span<MarkElement const> component_usage) const { return start_update_state; }
			void FlushSystemNodeMutex(StructLayoutMarksInfos component, StructLayoutMarksInfos singleton, StructLayoutMarksInfos thread_order) const {}
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

		StructLayoutMarksInfos component;
		StructLayoutMarksInfos singleton;
		StructLayoutMarksInfos thread_core;

		DynamicAutoSystemHolder(
				Context& context, Func fun, Potato::IR::MemoryResourceRecord record,
			StructLayoutMarksInfos component,
			StructLayoutMarksInfos singleton,
			StructLayoutMarksInfos thread_core
			)
			: append_data(context, 0), fun(std::move(fun)), MemoryResourceRecordIntrusiveInterface(record), component(component), singleton(singleton), thread_core(thread_core)
		{
			append_data.FlushSystemNodeMutex(component, singleton, thread_core);
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
		virtual SystemNode::ComponentOverlappingState IsComponentOverlapping(ComponentFilter const& target_component_filter, std::span<MarkElement const> archetype_update, std::span<MarkElement const> archetype_usage_count) const override
		{
			return append_data.IsComponentOverlapping(SystemNode::ComponentOverlappingState::NoUpdate, target_component_filter, archetype_update, archetype_usage_count);
		}
		virtual SystemNode::ComponentOverlappingState IsComponentOverlapping(SystemNode const& target_node, std::span<MarkElement const> archetype_update, std::span<MarkElement const> archetype_usage_count) const override
		{
			return append_data.IsComponentOverlapping(SystemNode::ComponentOverlappingState::NoUpdate, target_node, archetype_update, archetype_usage_count);
		}
		virtual SystemNode::Mutex GetMutex() const override
		{
			return {
				component,
				singleton,
				thread_core
			};
		}
	};

	template<typename Function>
	SystemNode::Ptr Context::CreateAutomaticSystem(Function&& func, std::pmr::memory_resource* resource)
	{
		using Type = DynamicAutoSystemHolder<std::remove_cvref_t<Function>>;
		auto layout = Potato::MemLayout::MemLayoutCPP::Get<Type>();
		auto span_offset = layout.Insert(Potato::IR::Layout::Get<MarkElement>(), 
			(component_manager.GetComponentMarkElementStorageCount() 
			+ singleton_manager.GetSingletonMarkElementStorageCount() 
			+ thread_order_manager.GetStorageCount()) * 2
		);
		auto re = Potato::IR::MemoryResourceRecord::Allocate(resource, layout.Get());
		if (re)
		{
			std::size_t ite_span_offset = span_offset;

			StructLayoutMarksInfos component{
				{new (re.GetByte(ite_span_offset)) MarkElement[component_manager.GetComponentMarkElementStorageCount()], component_manager.GetComponentMarkElementStorageCount()},
				{new (re.GetByte(ite_span_offset) + sizeof(MarkElement) * component_manager.GetComponentMarkElementStorageCount()) MarkElement[component_manager.GetComponentMarkElementStorageCount()], component_manager.GetComponentMarkElementStorageCount()},
			};

			ite_span_offset += (component_manager.GetComponentMarkElementStorageCount() * 2 * sizeof(MarkElement));

			StructLayoutMarksInfos singleton{
				{new (re.GetByte(ite_span_offset)) MarkElement[singleton_manager.GetSingletonMarkElementStorageCount()], singleton_manager.GetSingletonMarkElementStorageCount()},
				{new (re.GetByte(ite_span_offset) + sizeof(MarkElement) * singleton_manager.GetSingletonMarkElementStorageCount()) MarkElement[singleton_manager.GetSingletonMarkElementStorageCount()], singleton_manager.GetSingletonMarkElementStorageCount()},
			};

			ite_span_offset += (singleton_manager.GetSingletonMarkElementStorageCount() * 2 * sizeof(MarkElement));

			StructLayoutMarksInfos thread_order{
				{new (re.GetByte(ite_span_offset)) MarkElement[thread_order_manager.GetStorageCount()], thread_order_manager.GetStorageCount()},
				{new (re.GetByte(ite_span_offset) + sizeof(MarkElement) * thread_order_manager.GetStorageCount()) MarkElement[thread_order_manager.GetStorageCount()], thread_order_manager.GetStorageCount()},
			};

			return new (re.Get()) Type{ *this, std::forward<Function>(func), re, component, singleton, thread_order };
		}
		return {};
	}

	template<typename Func>
	struct SystemTemplateHolder : public SystemTemplate, public Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		Func func;
		virtual SystemNode::Ptr CreateSystemNode(Context& context, std::pmr::memory_resource* resource) const override
		{
			return context.CreateAutomaticSystem(func, resource);
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
}
