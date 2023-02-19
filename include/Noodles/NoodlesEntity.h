#pragma once
#include "NoodlesMemory.h"
#include <mutex>

namespace Noodles
{


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