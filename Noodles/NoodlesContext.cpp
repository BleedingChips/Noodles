module;

module NoodlesContext;

namespace Noodles
{

	auto Context::Create(Config config, Potato::Task::TaskContext::Ptr TaskPtr, std::pmr::memory_resource* UpstreamResource)->Ptr
	{
		if(TaskPtr && UpstreamResource != nullptr)
		{
			auto Adress = UpstreamResource->allocate(sizeof(Context), alignof(Context));
			if(Adress != nullptr)
			{
				return new (Adress) Context{ config, std::move(TaskPtr), UpstreamResource };
			}else
			{
				return {};
			}
		}
		return {};
	}

	Context::Context(Config config, Potato::Task::TaskContext::Ptr TaskPtr, std::pmr::memory_resource* Resource)
		: TaskContext(std::move(TaskPtr)), MemoryResource(Resource),
	EntityResource(Memory::HugePageMemoryResource::Create(Resource)),
	ArcheTypeResource(Resource),
	ComponentResource(Resource), config(config)
	{
		volatile int i = 0;
	}

	Context::~Context()
	{
		
	}

	void Context::operator()(Potato::Task::ExecuteStatus& Status)
	{
		std::lock_guard lg(property_mutex);

		if(TaskContext)
		{
			std::chrono::time_zone tz{
				std::string_view{"Asia/Shanghai"}
			};
			auto cur = std::chrono::system_clock::now();
			//cur.
			auto ltime = tz.to_local(cur);
			//std::chrono::local_days ld{ltime};
			//std::println("Context : H:{0}, M:{1}, S:{2}", time);


			//TaskContext->Com
			
		}

	}

	void Context::Release()
	{
		auto OldResource = MemoryResource;
		this->~Context();
		OldResource->deallocate(
			this,
			sizeof(Context),
			alignof(Context)
		);
	}

	

}