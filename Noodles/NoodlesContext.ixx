module;

export module NoodlesContext;

import std;

import PotatoMisc;
import PotatoPointer;
export import PotatoTaskSystem;

import NoodlesMemory;
export import NoodlesSystem;
//import NoodlesEntity;



export namespace Noodles
{

	struct Context;

	struct ExecuteContext
	{
		System::Property property;
		Context& context;
	};

	struct Context final : public Potato::Task::Task
	{

		struct Config
		{
			std::size_t priority = *Potato::Task::TaskPriority::Normal;
			std::chrono::milliseconds min_frame_time = std::chrono::milliseconds{13};
		};

		using Ptr = Potato::Task::ControlPtr<Context>;

		static Ptr Create(Config config, Potato::Task::TaskContext::Ptr ptr, std::pmr::memory_resource* UpstreamResource = std::pmr::get_default_resource());

		bool StartLoop();

		bool AddRawTickSystem(
			System::Priority priority,
			System::Property sys_property,
			System::MutexProperty mutex_property,
			System::Object&& obj
			);

	protected:

		virtual void operator()(Potato::Task::ExecuteStatus& Status) override;

		Context(Config config, Potato::Task::TaskContext::Ptr TaskPtr, std::pmr::memory_resource* Resource);

		//virtual void ControlRelease() override;
		virtual void Release() override;
		~Context();

		void InitTickSystem();
		void FlushTickSystem();
		

		std::optional<std::size_t> CheckConflict(System::Priority priority,
			System::Property sys_property,
			System::MutexProperty mutex_property);

		std::atomic_size_t running_task;

		std::mutex property_mutex;
		Potato::Task::TaskContext::Ptr task_context;
		Config config;
		std::chrono::system_clock::time_point last_execute_time;

		std::pmr::memory_resource* m_resource;
		/*
		Memory::HugePageMemoryResource::Ptr EntityResource;
		std::pmr::synchronized_pool_resource ArcheTypeResource;
		std::pmr::synchronized_pool_resource ComponentResource;
		std::pmr::synchronized_pool_resource SystemResource;
		*/

		struct SystemStorage
		{
			System::Priority priority;
			System::Property property;
			System::MutexProperty mutex_property;
			System::Object object;
			std::size_t in_degree = 0;
		};

		enum class SystemStatus
		{
			Waitting,
			Ready,
			Running,
			Done,
		};

		struct GraphicLine
		{
			bool is_mutex = false;
			std::int32_t layer = 0;
			std::size_t from_node = 0;
			std::size_t to_node = 0;
		};

		struct SystemRunningContext
		{
			SystemStatus status = SystemStatus::Ready;
			System::Object::Ref object;
			System::Property property;
			std::size_t layer = 0;
			std::span<GraphicLine const> graphic_line;
			std::span<GraphicLine const> reverse_graphic_line;
			std::size_t in_degree = 0;
		};

		void FireSingleTickSystem(Potato::Task::TaskContext& context, std::size_t cur_index);
		void TryFireNextLevelZeroDegreeTickSystem(Potato::Task::TaskContext& context, std::optional<std::size_t> current_context_index);
		void TryFireBeDependenceTickSystem(Potato::Task::TaskContext& context, std::size_t start_ite);

		std::mutex tick_system_mutex;
		std::pmr::vector<SystemStorage> tick_systems;
		std::pmr::vector<GraphicLine> tick_systems_graphic_line;
		bool need_refresh_dependence = false;

		std::mutex tick_system_running_mutex;
		std::pmr::vector<SystemRunningContext> tick_system_context;
		std::pmr::vector<GraphicLine> tick_systems_running_graphic_line;
		std::size_t current_level_system_waiting = 0;
		
	};
}

namespace Noodles
{
	
}