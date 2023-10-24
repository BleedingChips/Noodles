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

	/*
	struct ContextConfig
	{
		std::size_t priority = *Potato::Task::TaskPriority::Normal;
		std::chrono::milliseconds min_frame_time = std::chrono::milliseconds{ 13 };
	};

	struct Context : public Potato::Task::Task
	{

		using Ptr = Potato::Task::ControlPtr<Context>;

		static Ptr Create(ContextConfig config, Potato::Task::TaskContext::Ptr ptr, std::pmr::memory_resource* UpstreamResource = std::pmr::get_default_resource());

		bool StartLoop();

		template<typename Func>
		bool AddTickSystemDefer(
			System::Priority priority,
			System::Property sys_property,
			System::MutexProperty mutex_property,
			Func&& fun
		)
		{
			std::lock_guard lg(tick_system_mutex);
			auto re = TickSystemDependenceCheck(
				sys_property,
				priority,
				mutex_property
			);
			if(re)
			{
				auto context = System::CreateObjFromCallableObject(std::forward<Func>(fun), &system_running_context_resource);
				if(context != nullptr)
				{
					TickSystemInsert(re, sys_property, priority, mutex_property, context);
					return true;
				}
			}
			return false;
		}

	protected:

		virtual void operator()(Potato::Task::ExecuteStatus& Status) override;

		Context(ContextConfig config, Potato::Task::TaskContext::Ptr TaskPtr, std::pmr::memory_resource* Resource);

		//virtual void ControlRelease() override;
		virtual void Release() override;
		~Context();

		void FlushAndInitTickSystem();

		std::atomic_size_t running_task;

		std::mutex property_mutex;
		Potato::Task::TaskContext::Ptr task_context;
		ContextConfig config;
		std::chrono::system_clock::time_point last_execute_time;

		std::pmr::memory_resource* m_resource;
		std::pmr::synchronized_pool_resource system_running_context_resource;

		struct NewLogicDependenceLine
		{
			bool is_mutex = false;
			std::size_t from_node = 0;
			std::size_t to_node = 0;
		};

		struct CircleDependenceCheckResult
		{
			enum class Status
			{
				Available,
				EmptyName,
				ExistName,
				ConfuseDependence,
				CircleDependence,
			};

			Status status = Status::Available;
			std::size_t ite_index = 0;
			std::size_t in_degree = 0;
			std::pmr::vector<NewLogicDependenceLine> dependence_line;
			operator bool() const { return status == Status::Available; }
		};

		CircleDependenceCheckResult TickSystemDependenceCheck(System::Property const& pro, System::Priority const& priority, System::MutexProperty const& mutex_property);
		void TickSystemInsert(CircleDependenceCheckResult const& result, System::Property const& pro, System::Priority const& priority, System::MutexProperty const& mutex_property, System::RunningContext* context);

		struct LogicDependenceLine
		{
			bool is_mutex = false;
			std::size_t to_node = 0;
		};

		struct LogicSystemRunningContext
		{
			System::Property property;
			System::Priority priority;
			System::MutexProperty mutex_property;
			System::RunningContext* system_obj;
			std::size_t in_degree = 0;
			std::pmr::vector<LogicDependenceLine> dependence_line;
		};

		struct StartupSystemContext
		{
			std::int32_t layout = 0;
			System::RunningContext* system_obj;
		};

		void FireSingleTickSystem(Potato::Task::TaskContext& context, System::RunningContext* ptr);
		void TryFireNextLevelZeroDegreeTickSystem(Potato::Task::TaskContext& context);
		void TryFireBeDependenceTickSystem(Potato::Task::TaskContext& context, System::RunningContext* ptr);
		bool RecursionSearchNode(std::size_t cur, std::size_t target, std::span<NewLogicDependenceLine> Line);

		std::mutex tick_system_mutex;
		std::pmr::vector<LogicSystemRunningContext> tick_systems;
		bool need_refresh_dependence = false;

		std::mutex tick_system_running_mutex;
		std::pmr::vector<StartupSystemContext> startup_system_context;
		std::size_t starup_up_system_context_ite = 0;
		std::pmr::vector<System::TriggerLine> tick_systems_running_graphic_line;
		std::size_t current_level_system_waiting = 0;
		
	};

	namespace System
	{
		export template<typename ...ToT>
			struct ExecuteContextDistributor<ComponentFilter<ToT...>>
		{
			ComponentFilter<ToT...> operator()(ExecuteContext& context) { return {}; }
		};
	}

	using ExecuteContext = System::ExecuteContext;
	*/

}

export namespace Noodles
{
	
}