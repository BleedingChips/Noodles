module;
#include <cassert>

module NoodlesEntity;

namespace Noodles
{
	/*
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
		assert(resource != nullptr);
		auto old_resource = resource;
		this->~EntityStorage();
		old_resource->deallocate(this, sizeof(EntityStorage), alignof(EntityStorage));
	}

	EntityStorage::EntityStorage(std::pmr::memory_resource* resource)
		: resource(resource)
	{
		
	}
	*/

}