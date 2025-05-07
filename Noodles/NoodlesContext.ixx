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
	

	export struct Context;

	struct RWClassBitFlagConstViewer
	{
		BitFlagContainerConstViewer read_bitflag;
		BitFlagContainerConstViewer write_bitflag;
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
		};

		struct Parameter
		{
			std::wstring_view system_name;
			std::int32_t layer = 0;
			Priority priority;
			std::wstring_view module_name;
		};


		struct ClassBitFlag
		{
			RWClassBitFlagConstViewer component;
			RWClassBitFlagConstViewer singleton;
			RWClassBitFlagConstViewer thread_order;
		};

		virtual ClassBitFlag GetClassBitFlag() const { return {}; };

		enum class ComponentOverlappingState
		{
			NoUpdate,
			IsOverlapped,
			IsNotOverlapped
		};

		//virtual ComponentOverlappingState IsComponentOverlapping(SystemNode const& target_node, std::span<MarkElement const> archetype_update, std::span<MarkElement const> archetype_usage_count) const { return ComponentOverlappingState::NoUpdate; };
		//virtual ComponentOverlappingState IsComponentOverlapping(ComponentQuery const& target_component_filter, std::span<MarkElement const> archetype_update, std::span<MarkElement const> archetype_usage_count) const { return ComponentOverlappingState::NoUpdate; };
		virtual bool UpdateQuery(Context& context) { return false; }
		
		struct RWClassBitFlagViewer
		{
			BitFlagContainerConstViewer usagne_class;
			BitFlagContainerConstViewer writed_class;
		};

		struct SystemClassBitFlagViewer
		{
			RWClassBitFlagViewer component;
			RWClassBitFlagViewer singleton;
			RWClassBitFlagViewer exclusion;
		};

		virtual SystemClassBitFlagViewer GetSystemClassBitFlagViewer() const { return {}; }

	protected:

		virtual bool IsComponentQueryConflict(SystemNode& target, BitFlagContainerConstViewer archetype_usage) { return false; }
		virtual bool IsComponentQueryConflict(BitFlagContainerConstViewer component_usage, BitFlagContainerConstViewer component_writed, BitFlagContainerConstViewer query_archetype_usage, BitFlagContainerConstViewer archetype_usage) { return false; }

		virtual void SystemNodeExecute(Context& context) = 0;


		virtual void AddSystemNodeRef() const = 0;
		virtual void SubSystemNodeRef() const = 0;

		virtual void AddTaskGraphicNodeRef() const override final { AddSystemNodeRef(); }
		virtual void SubTaskGraphicNodeRef() const override final { SubSystemNodeRef(); }

	private:
		
		virtual void TaskFlowNodeExecute(Potato::Task::Context& context, Potato::TaskFlow::Controller& controller) override {}

		friend struct Context;
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
		std::size_t GetExclusionBitFlagContainerCount() const { return exclusion_map.GetBitFlagContainerElementCount(); }
		std::size_t GetCurrentFrameCount() const { std::shared_lock sl{ info_mutex }; return frame_count; }
		std::chrono::duration<float> GetDeltaTime() const { std::shared_lock sl(info_mutex); return delta_time; }

		//float GetDeltaTimeInSecond() const { std::sha }

		struct Parameter
		{
			std::wstring_view instance_name = L"NoodlesInstance";
			std::chrono::milliseconds duration_time = std::chrono::milliseconds{ 30 };
		};

		virtual bool Commit(Potato::Task::Context& context, Parameter parameter = {});

		virtual bool AddSystemNode(SystemNode::Ptr node, SystemNode::Parameter parameter);

	protected:

		Instance(Config config, std::pmr::memory_resource* resource);

		virtual void BeginFlow(Potato::Task::Context& context, Potato::Task::Node::Parameter parameter) override;
		virtual void FinishFlow_AssumedLocked(Potato::Task::Context& context, Potato::Task::Node::Parameter parameter) override;
		virtual bool UpdateFlow_AssumedLocked(std::pmr::memory_resource* resource = std::pmr::get_default_resource());
		virtual void ExecuteNode(Potato::Task::Context& context, Potato::TaskFlow::Node& node, Potato::TaskFlow::Controller& controller) override;
		//virtual void EndFlow(Potato::Task::Context& context, Potato::Task::Node::Parameter parameter) override;

		mutable std::shared_mutex info_mutex;
		std::chrono::steady_clock::time_point startup_time;
		std::size_t frame_count = 0;
		std::chrono::duration<float> delta_time;

		AsynClassBitFlagMap component_map;
		AsynClassBitFlagMap singleton_map;
		AsynClassBitFlagMap exclusion_map;

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
		bool need_update = false;

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
			bool available = false;
			Potato::TaskFlow::Flow::NodeIndex index;
			SystemNode::Parameter parameter;
		};
		std::pmr::vector<SystemNodeInfo> system_info;

	private:

		friend struct Ptr::CurrentWrapper;
		friend struct Ptr;
	};



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
