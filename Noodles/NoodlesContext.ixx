module;

export module NoodlesContext;

import std;
import PotatoMisc;
import PotatoPointer;
import PotatoTaskSystem;
import NoodlesMemory;
import NoodlesEntity;


export namespace Noodles
{

	struct EntityPolicy
	{
		//virtual void Create();
	};

	struct Context : Potato::Task::Task
	{
		using Ptr = Potato::Task::ControlPtr<Context>;

		static Ptr Create(Potato::Task::TaskContext::Ptr Ptr, std::pmr::memory_resource* UpstreamResource = std::pmr::get_default_resource());

		Entity CreateEntity(EntityPolicy const& Policy);

	protected:

		virtual void operator()(Potato::Task::ExecuteStatus Status, Potato::Task::TaskContext&) override;

		Context(Potato::Task::TaskContext::Ptr TaskPtr, std::pmr::memory_resource* Resource);

		//virtual void ControlRelease() override;
		virtual void Release() override;
		~Context();

		Potato::Task::TaskContext::Ptr TaskContext;

		std::pmr::memory_resource* MemoryResource;
		Memory::HugePageMemoryResource::Ptr EntityResource;
		std::pmr::synchronized_pool_resource ArcheTypeResource;
		std::pmr::synchronized_pool_resource ComponentResource;
	};


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