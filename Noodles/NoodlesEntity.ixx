module;

export module NoodlesEntity;

import std;
import PotatoMisc;
import PotatoPointer;

//import NoodlesArcheType;

export namespace Noodles
{

	struct EntityStorage : public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<EntityStorage>;

	protected:

		EntityStorage(std::pmr::memory_resource* Resource);

		static Ptr Create(std::pmr::memory_resource* Resource = std::pmr::get_default_resource());
		virtual void Release() override;

		std::pmr::memory_resource* Resource;

		/*
		std::mutex CurrentDataMutex;
		ArcheType::Ptr ArcheType;


		std::shared_mutex TemporaryDataMutex;
		ArcheType::Ptr TempArcheType;
		*/

		friend struct Context;
	};

	using Entity = EntityStorage::Ptr;



	/*
	struct EntityManager
	{
		using Ptr = Potato::Pointer::IntrusivePtr<EntityManager>;

		static Ptr Create(std::pmr::memory_resource* Resource = std::pmr::get_default_resource());

		struct Storage
		{
			using SPtr = Potato::Pointer::StrongPtr<EntityStorage>;
		private:

			void AddRef() { Ref.AddRef(); }
			void SubRef();

			Potato::Misc::AtomicRefCount Ref;
			friend struct Potato::Pointer::IntrusiveSubWrapperT;
		};

		

		Entity Create();

	private:

		EntityManager(std::pmr::memory_resource* Uppder);

		void AddRef();
		void SubRef();

		std::mutex ResourceMutex;
		std::pmr::unsynchronized_pool_resource MemoryResource;
		Potato::Misc::AtomicRefCount RefCount;

		friend struct Potato::Pointer::IntrusiveSubWrapperT;
	};
	*/

	
	
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