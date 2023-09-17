module;

module NoodlesContext;

namespace Noodles
{

	auto Context::Create(Potato::Task::TaskContext::Ptr TaskPtr, std::pmr::memory_resource* UpstreamResource)->Ptr
	{
		if(TaskPtr && UpstreamResource != nullptr)
		{
			auto Adress = UpstreamResource->allocate(sizeof(Context), alignof(Context));
			if(Adress != nullptr)
			{
				return new (Adress) Context{ std::move(TaskPtr), UpstreamResource };
			}else
			{
				return {};
			}
		}
		return {};
	}

	Context::Context(Potato::Task::TaskContext::Ptr TaskPtr, std::pmr::memory_resource* Resource)
		: TaskContext(std::move(TaskPtr)), MemoryResource(Resource),
	EntityResource(Memory::HugePageMemoryResource::Create(Resource)),
	ArcheTypeResource(Resource),
	ComponentResource(Resource)
	{
		volatile int i = 0;
	}

	Context::~Context()
	{
		
	}

	void Context::operator()(Potato::Task::ExecuteStatus Status, Potato::Task::TaskContext& Context)
	{
		
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

	Entity Context::CreateEntity(EntityPolicy const& Policy)
	{
		return EntityStorage::Create(EntityResource.GetPointer());
	}

	/*
	struct EntityManager
	{

		using Ptr = Potato::Misc::IntrusivePtr<EntityManager>;

		static auto CreateInstance(std::size_t UniqueContentID, std::size_t MinEntityCountInOnePage) -> Ptr;

		struct EntityStorge
		{
			bool InUsed;
			std::byte* Buffer;
		};

		struct EntityStorage
		{
			std::mutex Mutex;
			mutable Potato::Misc::AtomicRefCount Ref;
		};


		struct EntityChunk
		{
			void AddRef() const { Ref.AddRef(); }
			void SubRef() const;

			mutable Potato::Misc::AtomicRefCount Ref;

			std::span<EntityStorge> Storages;
		};

	private:

		EntityManager(std::size_t UniqueID, std::size_t MinEntityCount) : UniqueID(UniqueID) {}
		
		Noodles::Memory::HugePage::Ptr Owner;
		std::size_t const UniqueID;
		std::mutex Mutex;
		mutable Potato::Misc::AtomicRefCount Ref;
	};

	struct Entity
	{
		
	};
	*/

}