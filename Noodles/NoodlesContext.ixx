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
		BitFlagContainerViewer archetype_usage;
	};

	enum class SystemCategory
	{
		Tick,
		Once,
		OnceNextFrame,
		OnceIgnoreLayer
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

	struct SubFlowSystemNode;
	struct EndingSystemNode;

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
		using ExecutedSystemIndex = Potato::Misc::VersionIndex;

		SystemIndex PrepareSystemNode(SystemNode::Ptr index, bool temporary = false);
		std::optional<ExecutedSystemIndex> LoadSystemNode(SystemCategory category, SystemIndex index, SystemNode::Parameter parameter = {});
		std::optional<ExecutedSystemIndex> LoadSystemNode(Context& context, SystemCategory category, SystemIndex index, SystemNode::Parameter parameter = {});
		decltype(auto) CreateEntity() {
			std::lock_guard lg(entity_mutex);
			return entity_manager.CreateEntity();
		}

		template<typename ComponentT>
		decltype(auto) AddEntityComponent(Entity& entity, ComponentT&& component) 
		{
			BitFlag component_bit_flag = *component_map.LocateOrAdd<ComponentT>();
			std::lock_guard lg(entity_mutex);
			return entity_manager.AddEntityComponent(entity, std::forward<ComponentT>(component), component_bit_flag);
		}

		template<typename SingletonT>
		bool AddSingleton(SingletonT&& singleton)
		{
			std::lock_guard lg(singleton_modify_mutex);
			return singleton_modify_manager.AddSingleton(std::forward<SingletonT>(singleton), singleton_map);
		}

	protected:

		using IndexSpan = Potato::Misc::IndexSpan<>;

		Instance(Config config, std::pmr::memory_resource* resource);

		std::tuple<std::size_t, std::size_t> GetQuery(std::size_t system_index, std::span<ComponentQuery::OPtr> component_query, std::span<SingletonQuery::OPtr> singleton_query);

		SystemRequireBitFlagViewer GetSystemRequireBitFlagViewer_AssumedLocked(std::size_t system_info_index);
		bool DetectSystemComponentOverlapping_AssumedLocked(std::size_t system_index, std::size_t system_index2);

		virtual void BeginFlow(Potato::Task::Context& context, Potato::Task::Node::Parameter parameter) override;
		virtual void FinishFlow_AssumedLocked(Potato::Task::Context& context, Potato::Task::Node::Parameter parameter) override;
		virtual bool UpdateFlow_AssumedLocked(std::pmr::memory_resource* resource = std::pmr::get_default_resource());
		virtual void UpdateSystems();
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

		Potato::TaskFlow::Flow::NodeIndex ending_system_index;

		std::mutex flow_mutex;
		Potato::TaskFlow::Flow main_flow;
		bool flow_need_update = true;
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
			bool has_temporary_last_frame = false;
			std::size_t execute_node_last_frame = 0;
		};

		struct ExecuteSystemNodeInfo
		{
			std::size_t version = 0;
			bool has_dynamic_edges = false;
			std::optional<SystemCategory> category;
			Potato::TaskFlow::Flow::NodeIndex flow_index;
			std::size_t system_info_index = 0;
			std::size_t sub_flow_index = 0;
			SystemNode::Parameter parameter;
		};

		std::shared_mutex system_mutex;
		std::pmr::vector<SystemNodeInfo> system_info;
		std::pmr::vector<BitFlagContainer::Element> system_bitflag_container;
		std::pmr::vector<ComponentQuery::Ptr> component_query;
		std::pmr::vector<SingletonQuery::Ptr> singleton_query;

		std::mutex execute_system_mutex;
		bool execute_system_need_update = true;
		std::pmr::vector<ExecuteSystemNodeInfo> execute_system_info;
		bool has_template_system = false;

		struct DynamicEdge
		{
			std::size_t from_system_index;
			std::size_t from_execute_system_index;
			std::size_t to_system_index;
			std::size_t to_execute_system_index;
			bool is_mutex = false;
			bool added = false;
		};

		std::pmr::vector<DynamicEdge> dynamic_edges;

		struct OnceSystemInfo
		{
			SystemNode::Parameter parameter;
			SystemIndex index;
		};

		std::mutex once_system_mutex;
		std::pmr::vector<OnceSystemInfo> once_system_node;
		std::int32_t current_layer = std::numeric_limits<std::int32_t>::max();
		std::size_t current_frame_once_system_iterator = 0;
		std::size_t current_frame_once_system_count = 0;

	private:

		friend struct Ptr::CurrentWrapper;
		friend struct Ptr;
		friend struct SystemInitializer;
		friend struct Context;
		friend struct SubFlowSystemNode;
		friend struct EndingSystemNode;
	};

	template<typename Type>
	concept IsThreadSafeType = requires(Type)
	{
		typename Type::NoodlesThreadSafeType;
	};

	template<typename Type>
	concept IsQueryWriteType = std::is_same_v<Type, std::remove_cvref_t<Type>> && !IsThreadSafeType<Type>;

	template<typename Type>
	concept IsQueryReadType = std::is_same_v<Type, std::add_const_t<std::remove_cvref_t<Type>>> || IsThreadSafeType<Type>;

	template<typename Type>
	concept AcceptableQueryType = std::is_same_v<Type, std::remove_cvref_t<Type>> || std::is_same_v<Type, std::add_const_t<std::remove_cvref_t<Type>>>;

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

		std::optional<Instance::ExecuteSystemIndex> LoadSystemNode(SystemCategory category, Instance::SystemIndex system_index, SystemNode::Parameter parameter = {})
		{
			return instance.LoadSystemNode(*this, category, system_index, std::move(parameter));
		}

		std::optional<std::size_t> QueryComponentArray(ComponentQuery const& query, std::size_t archetype_index, std::size_t chunk_index, std::span<void*> output)
		{
			return query.QueryComponentArrayWithIterator(instance.component_manager, archetype_index, chunk_index, output);
		}

		bool QueryEntity(ComponentQuery const& query, Entity const& entity, std::span<void*> output)
		{
			auto index = entity.GetEntityIndex();
			if (index.has_value())
			{
				return query.QueryComponent(instance.component_manager, *index, output);
			}
			return false;
		}

		bool QuerySingleton(SingletonQuery const& query, std::span<void*> output)
		{
			return query.QuerySingleton(instance.singleton_manager, output);
		}

		Instance& GetInstance() const { return instance; }

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
		friend struct SubFlowSystemNode;
		friend struct EndingSystemNode;
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
}
