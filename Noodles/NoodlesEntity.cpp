module;
#include <cassert>

module NoodlesEntity;

namespace Noodles
{
	auto EntityStorage::Create(std::pmr::memory_resource* Resource)
		-> Ptr
	{
		assert(Resource != nullptr);
		auto Adress = Resource->allocate(sizeof(EntityStorage), alignof(EntityStorage));
		if (Adress != nullptr)
		{
			Ptr TPtr{ new (Adress) EntityStorage{ Resource } };
			return TPtr;
		}
		return {};
	}

	void EntityStorage::Release()
	{
		auto OldResource = Resource;
		this->~EntityStorage();
		OldResource->deallocate(this, sizeof(EntityStorage), alignof(EntityStorage));
	}

	EntityStorage::EntityStorage(std::pmr::memory_resource* Resource)
		: Resource(Resource)
	{
		
	}

	/*
	auto EntityManager::Create(std::pmr::memory_resource* Resource) -> Ptr
	{
		assert(Resource != nullptr);
		auto Adress = Resource->allocate(sizeof(EntityManager), alignof(EntityManager));
		if(Adress != nullptr)
		{
			Ptr TPtr{new (Adress) EntityManager{ Resource }};
			return TPtr;
		}
		return {};
	}
	*/


}