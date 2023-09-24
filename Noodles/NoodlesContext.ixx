module;

export module NoodlesContext;

import std;

import PotatoMisc;
import PotatoPointer;
import PotatoTaskSystem;

import NoodlesMemory;
import NoodlesSystem;
//import NoodlesEntity;



export namespace Noodles
{

	struct Context final : public Potato::Task::Task
	{

		struct Config
		{
			std::size_t MaxFPS = 140;
		};

		using Ptr = Potato::Task::ControlPtr<Context>;

		static Ptr Create(Config config, Potato::Task::TaskContext::Ptr ptr, std::pmr::memory_resource* UpstreamResource = std::pmr::get_default_resource());

		struct ExecuteStatus
		{
			Context& context;
		};

		void StartLoop();

		bool AddRawSystem(
			void (*sysfunc)(void* object, ExecuteStatus& status),
			void* object,
			std::span<SystemRWInfo const> Infos, 
			SystemProperty system_property, 
			std::strong_ordering (*priority_detect)(void* Object, SystemProperty& )
			);

	protected:

		virtual void operator()(Potato::Task::ExecuteStatus& Status) override;

		Context(Config config, Potato::Task::TaskContext::Ptr TaskPtr, std::pmr::memory_resource* Resource);

		//virtual void ControlRelease() override;
		virtual void Release() override;
		~Context();

		
		std::mutex property_mutex;
		std::chrono::system_clock::time_point current_time;
		Potato::Task::TaskContext::Ptr TaskContext;
		Config config;

		std::pmr::memory_resource* MemoryResource;


		Memory::HugePageMemoryResource::Ptr EntityResource;
		std::pmr::synchronized_pool_resource ArcheTypeResource;
		std::pmr::synchronized_pool_resource ComponentResource;
		std::pmr::synchronized_pool_resource SystemResource;

		enum class SystemExecuteStatus
		{
			Ready,
			Waiting,
			Running,
			Abandon
		};

		struct SystemBlock
		{
			SystemExecuteStatus status;
			SystemProperty property;
			SystemObject object;
		};

		

		std::mutex system_mutex;
		std::pmr::vector<SystemBlock> systems;
		std::pmr::vector<SystemBlock> immediately_system;
		std::pmr::vector<SystemBlock> temporary_systems;
	};

}