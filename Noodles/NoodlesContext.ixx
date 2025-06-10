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
import NoodlesQuery;
import NoodlesBitFlag;
import NoodlesClassBitFlag;


export namespace Noodles
{
	constexpr auto InstanceLogCategory = Potato::Log::LogCategory(L"Noodles");

	export struct Context;
	export struct SystemInitializer;

	struct RequireBitFlagViewer
	{
		BitFlagContainerViewer require;
		BitFlagContainerViewer write;
	};

	struct SystemRequireBitFlagViewer
	{
		RequireBitFlagViewer component;
		RequireBitFlagViewer singleton;
	};

	enum class SystemCategory
	{
		Tick,
		Once,
		OnceNextFrame,
	};

	struct SystemNode : public Potato::TaskFlow::Node
	{

		struct Wrapper
		{
			void AddRef(SystemNode const* ptr) { ptr->AddSystemNodeRef(); }
			void SubRef(SystemNode const* ptr) { ptr->SubSystemNodeRef(); }
		};

		using Ptr = Potato::Pointer::IntrusivePtr<SystemNode, Wrapper>;

		struct Priority
		{
			std::int32_t primary = 0;
			std::int32_t second = 0;
			std::int32_t third = 0;
			std::strong_ordering operator<=>(Priority const&) const = default;
			bool operator==(Priority const&) const = default;
		};

		struct Parameter
		{
			std::wstring_view name;
			std::int32_t layer = 0;
			Priority priority;
			std::wstring_view module_name;
		};

	protected:

		virtual void Init(SystemInitializer& initializer) {}

		virtual void SystemNodeExecute(Context& context) = 0;


		virtual void AddSystemNodeRef() const = 0;
		virtual void SubSystemNodeRef() const = 0;

		virtual void AddTaskGraphicNodeRef() const override final { AddSystemNodeRef(); }
		virtual void SubTaskGraphicNodeRef() const override final { SubSystemNodeRef(); }

	private:
		
		virtual void TaskFlowNodeExecute(Potato::Task::Context& context, Potato::TaskFlow::Controller& controller) override {};

		friend struct Context;
		friend struct Instance;
		friend struct LayerTaskFlow;
		friend struct ParallelExecutor;

		friend struct Ptr::CurrentWrapper;
	};

	struct Instance : protected Potato::TaskFlow::Executor
	{
		struct Config
		{
			std::size_t component_class_count = 128;
			std::size_t singleton_class_count = 128;
			std::size_t exclusion_class_count = 128;
			std::size_t max_archetype_count = 128;
		};


		using Ptr = Potato::Pointer::IntrusivePtr<Instance, Executor::Wrapper>;

		static Ptr Create(Config config = {}, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		std::size_t GetSingletonBitFlagContainerCount() const { return singleton_map.GetBitFlagContainerElementCount(); }
		std::size_t GetComponentBitFlagContainerCount() const { return component_map.GetBitFlagContainerElementCount(); }
		std::size_t GetCurrentFrameCount() const { return frame_count; }
		std::chrono::duration<float> GetDeltaTime() const { return delta_time; }

		//float GetDeltaTimeInSecond() const { std::sha }

		struct Parameter
		{
			std::wstring_view instance_name = L"NoodlesInstance";
			std::chrono::milliseconds duration_time = std::chrono::milliseconds{ 30 };
		};

		virtual bool Commit(Potato::Task::Context& context, Parameter parameter = {});

		using SystemIndex = Potato::Misc::VersionIndex;

		SystemIndex PrepareSystemNode(SystemNode::Ptr index, bool temporary = false);
		bool LoadSystemNode(SystemCategory category, SystemIndex index, SystemNode::Parameter parameter = {});
		bool LoadSystemNode(Potato::Task::Context& context, SystemCategory category, SystemIndex index, SystemNode::Parameter parameter = {});

	protected:

		using IndexSpan = Potato::Misc::IndexSpan<>;

		Instance(Config config, std::pmr::memory_resource* resource);

		std::tuple<std::size_t, std::size_t> GetQuery(std::size_t system_index, std::span<ComponentQuery::OPtr> component_query, std::span<SingletonQuery::OPtr> singleton_query);

		SystemRequireBitFlagViewer GetSystemRequireBitFlagViewer_AssumedLocked(std::size_t system_info_index);

		virtual void BeginFlow(Potato::Task::Context& context, Potato::Task::Node::Parameter parameter) override;
		virtual void FinishFlow_AssumedLocked(Potato::Task::Context& context, Potato::Task::Node::Parameter parameter) override;
		virtual bool UpdateFlow_AssumedLocked(std::pmr::memory_resource* resource = std::pmr::get_default_resource());
		virtual void ExecuteNode(Potato::Task::Context& context, Potato::TaskFlow::Node& node, Potato::TaskFlow::Controller& controller) override;
		
		using ExecuteSystemIndex = Potato::Misc::VersionIndex;

		ExecuteSystemIndex FindAvailableSystemIndex_AssumedLocked();

		//virtual void EndFlow(Potato::Task::Context& context, Potato::Task::Node::Parameter parameter) override;

		std::atomic_size_t frame_count = 0;
		std::atomic<std::chrono::duration<float>> delta_time;

		mutable std::shared_mutex info_mutex;
		std::chrono::steady_clock::time_point startup_time;

		AsynClassBitFlagMap component_map;
		AsynClassBitFlagMap singleton_map;

		mutable std::shared_mutex component_mutex;
		ComponentManager component_manager;
		
		mutable std::mutex entity_mutex;
		EntityManager entity_manager;

		mutable std::shared_mutex singleton_mutex;
		SingletonManager singleton_manager;

		std::mutex singleton_modify_mutex;
		SingletonModifyManager singleton_modify_manager;

		std::mutex flow_mutex;
		Potato::TaskFlow::Flow main_flow;
		bool flow_need_update = false;

		struct SubFlowState
		{
			std::int32_t layer = 0;
			Potato::TaskFlow::Flow flow;
			bool need_update = false;
			Potato::TaskFlow::Flow::NodeIndex index;
		};
		std::pmr::vector<SubFlowState> sub_flows;
		

		struct SystemNodeInfo
		{
			std::size_t version = 0;
			SystemNode::Ptr node;
			IndexSpan component_query_index;
			IndexSpan singleton_query_index;
			bool temporary = false;
		};

		struct ExecuteSystemNodeInfo
		{
			std::optional<SystemCategory> category;
			Potato::TaskFlow::Flow::NodeIndex flow_index;
			SystemNode::Parameter parameter;
			std::size_t version = 0;
		};

		std::shared_mutex system_mutex;
		std::pmr::vector<SystemNodeInfo> system_info;
		std::pmr::vector<BitFlagContainer::Element> system_bitflag_container;
		std::pmr::vector<ComponentQuery::Ptr> component_query;
		std::pmr::vector<SingletonQuery::Ptr> singleton_query;

		std::mutex execute_system_mutex;
		std::pmr::vector<ExecuteSystemNodeInfo> execute_system_info;

		struct OnceSystemInfo
		{
			SystemNode::Parameter parameter;
			SystemIndex index;
		};

		std::pmr::vector<OnceSystemInfo> once_system_node;
		std::size_t current_frame_once_system_iterator = 0;
		std::size_t current_frame_once_system_count = 0;

	private:

		friend struct Ptr::CurrentWrapper;
		friend struct Ptr;
		friend struct SystemInitializer;
		friend struct Context;
	};

	template<typename Type>
	concept IsQueryWriteType = std::is_same_v<Type, std::remove_cvref_t<Type>>;

	template<typename Type>
	concept IsQueryReadType = std::is_same_v<Type, std::add_const_t<std::remove_cvref_t<Type>>>;

	template<typename Type>
	concept AcceptableQueryType = IsQueryWriteType<Type> || IsQueryReadType<Type>;

	struct SingletonQueryInitializer
	{
		SingletonQueryInitializer(std::span<BitFlag> require, BitFlagContainerViewer writed, AsynClassBitFlagMap& bitflag_map)
			: require(require), writed(writed), bitflag_map(bitflag_map) {
		}
		bool SetRequire(Potato::IR::StructLayout::Ptr struct_layout, bool is_writed);
		template<AcceptableQueryType RequireType>
		bool SetRequire() {
			return SetRequire(
				Potato::IR::StructLayout::GetStatic<std::remove_cvref_t<RequireType>>(),
				IsQueryWriteType<RequireType>
			);
		}
	protected:
		std::span<BitFlag> require;
		std::size_t iterator_index = 0;
		BitFlagContainerViewer writed;
		AsynClassBitFlagMap& bitflag_map;
	};

	struct ComponentQueryInitializer : public SingletonQueryInitializer
	{
		ComponentQueryInitializer(std::span<BitFlag> require, BitFlagContainerViewer writed, AsynClassBitFlagMap& bitflag_map, BitFlagContainerViewer refuse)
			: SingletonQueryInitializer(require, writed, bitflag_map), refuse(refuse)
		{}
		bool SetRefuse(Potato::IR::StructLayout::Ptr struct_layout);
		template<AcceptableQueryType RequireType>
		bool SetRefuse() {
			return SetRefuse(
				Potato::IR::StructLayout::GetStatic<std::remove_cvref_t<RequireType>>(),
				IsQueryWriteType<RequireType>
			);
		}
	protected:
		BitFlagContainerViewer refuse;
	};

	template<typename InitFunction>
	concept SystemInitializerComponentQueryInitFunction
		= std::is_invocable_v<
			std::remove_cvref_t<InitFunction>,
			ComponentQueryInitializer&
		>;

	template<typename InitFunction>
	concept SystemInitializerSingletonQueryInitFunction
		= std::is_invocable_v<
			std::remove_cvref_t<InitFunction>,
			SingletonQueryInitializer&
		>;


	struct SystemInitializer
	{

		struct Config
		{
			std::pmr::memory_resource* component_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* singleton_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* resource = std::pmr::get_default_resource();
		};

		template<SystemInitializerComponentQueryInitFunction InitFunction>
		OptionalSizeT CreateComponentQuery(std::size_t component_count, InitFunction&& func);

		template<SystemInitializerSingletonQueryInitFunction InitFunction>
		OptionalSizeT CreateSingletonQuery(std::size_t singleton_count, InitFunction&& func);

	protected:
		SystemInitializer(Instance& instance, Config config = {})
			: instance(instance), component_list(config.resource), singleton_list(config.resource),
			component_resource(config.component_resource), singleton_resource(config.singleton_resource),
			temp_resource(config.resource)
		{}

		Instance& instance;
		std::pmr::vector<ComponentQuery::Ptr> component_list;
		std::pmr::vector<SingletonQuery::Ptr> singleton_list;
		std::pmr::memory_resource* component_resource;
		std::pmr::memory_resource* singleton_resource;
		std::pmr::memory_resource* temp_resource;

		friend struct Instance;
	};

	struct Context
	{
		std::tuple<std::size_t, std::size_t> GetQuery(std::span<ComponentQuery::OPtr> component_query, std::span<SingletonQuery::OPtr> singleton_query)
		{
			return instance.GetQuery(system_index, component_query, singleton_query);
		}
	protected:
		Context(Potato::Task::Context& context, Potato::TaskFlow::Controller& controller, Instance& instance, std::size_t system_index)
			: context(context), controller(controller), instance(instance), system_index(system_index)
		{

		}

		Potato::Task::Context& context;
		Potato::TaskFlow::Controller& controller;
		Instance& instance;
		std::size_t system_index;

		friend struct SystemNode;
		friend struct Instance;
	};


	template<SystemInitializerComponentQueryInitFunction InitFunction>
	OptionalSizeT SystemInitializer::CreateComponentQuery(std::size_t component_count, InitFunction&& function)
	{
		auto ptr = ComponentQuery::Create(
			instance.component_manager.GetArchetypeBitFlagContainerCount(),
			instance.component_map.GetBitFlagContainerElementCount(),
			component_count,
			[&](std::span<BitFlag> output, BitFlagContainerViewer writed, BitFlagContainerViewer refuse) 
			{
				ComponentQueryInitializer initializer(output, writed, instance.component_map, refuse);
				function(initializer);
			},
			component_resource
		);
		if (ptr)
		{
			component_list.emplace_back(std::move(ptr));
			return component_list.size() - 1;
		}
		return {};
	}

	template<SystemInitializerSingletonQueryInitFunction InitFunction>
	OptionalSizeT SystemInitializer::CreateSingletonQuery(std::size_t singleton_count, InitFunction&& func)
	{
		auto ptr = SingletonQuery::Create(
			instance.singleton_map.GetBitFlagContainerElementCount(),
			singleton_count,
			[&](std::span<BitFlag> output, BitFlagContainerViewer writed)
			{
				SingletonQueryInitializer initializer(output, writed, instance.component_map);
				func(initializer);
			},
			singleton_resource
		);
		if (ptr)
		{
			singleton_list.emplace_back(std::move(ptr));
			return singleton_list.size() - 1;
		}
		return {};
	}

	/*
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

	

	export struct ContextWrapper
	{
		Context& GetContext() const { return context; }
		void AddTemporarySystemNodeNextFrame(SystemNode& node, Potato::Task::Property property);
		void AddTemporarySystemNode(SystemNode& node, Potato::Task::Property property);
		ContextWrapper(Potato::TaskGraphic::ContextWrapper& wrapper, Context& context, LayerTaskFlow& layer_flow)
			: wrapper(wrapper), context(context), layer_flow(layer_flow) {}
		Potato::Task::Property& GetNodeProperty() const { return wrapper.GetNodeProperty(); }
	protected:
		Potato::TaskGraphic::ContextWrapper& wrapper;
		Context& context;
		LayerTaskFlow& layer_flow;
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

	export struct LayerTaskFlow : protected Potato::TaskGraphic::Flow, protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<LayerTaskFlow, Flow::Wrapper>;

		bool AddTemporaryNode(SystemNode& node, Potato::Task::Property property);
		void AddTemporaryNodeNextFrame(SystemNode& node, Potato::Task::Property property);

	protected:

		
		bool AddTickedNode(SystemNode& node, Property property);
		virtual void TaskFlowExecuteBegin_AssumedLocked(Potato::Task::ContextWrapper& context) override;
		virtual void TaskFlowPostUpdateProcessNode_AssumedLocked(Potato::Task::ContextWrapper& wrapper) override;
		static bool TemporaryNodeDetect(Context& context, SystemNode& node, Potato::Task::Property& property, SystemNode const& t_node, Potato::Task::Property const& target_property);

		bool AddTemporaryNode_AssumedLocked(SystemNode& node, Potato::Task::Property property);

		LayerTaskFlow(Potato::IR::MemoryResourceRecord record, std::int32_t layout)
			: MemoryResourceRecordIntrusiveInterface(record), layout(layout)
		{
		}

		Context& GetContextFromTrigger();

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

		friend struct Flow::Wrapper;
		friend struct Ptr;
		friend struct Context;
		friend struct SystemNode;
		friend struct ContextWrapper;
		friend struct ParallelExecutor;
		friend struct LayerTaskFlowProcessContext;
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

		static Ptr Create(StructLayoutManager& manager, Config config = {}, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		Entity::Ptr CreateEntity() { return entity_manager.CreateEntity(); }

		template<typename Type>
		bool AddEntityComponent(Entity& entity, Type&& type) { return entity_manager.AddEntityComponent(entity, std::forward<Type>(type)); }

		bool RemoveSystemDefer(std::u8string_view system_name);
		bool AddTickedSystemNode(SystemNode& node, Property property);
		bool AddTemporarySystemNodeNextFrame(SystemNode& node, std::int32_t layout, Potato::Task::Property property);

		bool RemoveEntity(Entity::Ptr entity) { return entity_manager.ReleaseEntity(std::move(entity)); }

		template<typename Type>
		bool AddSingleton(Type&& type) { return singleton_manager.AddSingleton(std::forward<Type>(type)); }

		bool IterateComponent_AssumedLocked(ComponentQuery const& query, std::size_t ite_index, QueryData& accessor) const { return component_manager.ReadComponentRow_AssumedLocked(query, ite_index, accessor); }
		bool ReadEntity_AssumedLocked(Entity const& entity, ComponentQuery const& query, QueryData& accessor) const { { return entity_manager.ReadEntityComponents_AssumedLocked(component_manager, entity, query, accessor); } }
		//decltype(auto) ReadEntityDirect_AssumedLocked(Entity const& entity, ComponentFilter const& filter, std::span<void*> output, bool prefer_modifier = true) const { return manager.ReadEntityDirect_AssumedLocked(entity, filter, output, prefer_modifier); };
		bool ReadSingleton_AssumedLocked(SingletonQuery const& query, QueryData& accessor) const { return singleton_manager.ReadSingleton_AssumedLocked(query, accessor); }
		bool UpdateQuery(ComponentQuery& query) const { return component_manager.UpdateFilter_AssumedLocked(query); }
		bool UpdateQuery(SingletonQuery& query) const { return singleton_manager.UpdateFilter_AssumedLocked(query); }

		void Quit();

		
		std::chrono::steady_clock::duration GetFramedDuration() const { return framed_duration; }
		float GetFramedDurationInSecond() const
		{
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(GetFramedDuration());
			return ms.count() / 1000.0f;
		}

		StructLayoutManager& GetStructLayoutManager() const { return *manager; }

	protected:

		Context(StructLayoutManager& manager, Config config = {});

		LayerTaskFlow* FindSubContextTaskFlow_AssumedLocked(std::int32_t layer);
		LayerTaskFlow::Ptr FindOrCreateContextTaskFlow_AssumedLocked(std::int32_t layer);

		virtual void TaskFlowExecuteBegin_AssumedLocked(Potato::Task::ContextWrapper& context) override;
		virtual void TaskFlowExecuteEnd_AssumedLocked(Potato::Task::ContextWrapper& context) override;
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

		StructLayoutManager::Ptr manager;
		ComponentManager component_manager;
		EntityManager entity_manager;
		SingletonManager singleton_manager;

		friend struct Potato::TaskGraphic::Flow::Wrapper;
		friend struct SystemNode;
		friend struct ParallelExecutor;
		friend struct LayerTaskFlow;
	};
	*/
}
