module;

export module NoodlesContext;

import std;
import PotatoMisc;
import PotatoPointer;
import PotatoTaskSystem;
import NoodlesMemory;


namespace Noodles
{

	struct Context : Potato::Task::Task
	{
		using Ptr = Potato::Task::ControlPtr<Context>;

		static Ptr Create(std::pmr::memory_resource* UpstreamResource);
		static Ptr RegisterTask(Potato::Task::TaskContext::Ptr TaskContext);

	protected:

		virtual void operator()(Potato::Task::ExecuteStatus Status, Potato::Task::TaskContext&) override;

		Context(std::pmr::memory_resource* Resource);

		virtual void ControlRelease() override;

		~Context();
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