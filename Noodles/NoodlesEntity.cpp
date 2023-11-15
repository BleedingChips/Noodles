module;
#include <cassert>

module NoodlesEntity;

namespace Noodles
{

	auto Entity::Create(std::pmr::memory_resource* Resource)
		-> Ptr
	{
		assert(Resource != nullptr);
		auto Adress = Resource->allocate(sizeof(Entity), alignof(Entity));
		if (Adress != nullptr)
		{
			Ptr TPtr{ new (Adress) Entity{ Resource } };
			return TPtr;
		}
		return {};
	}

	void Entity::Release()
	{
		assert(resource != nullptr);
		auto old_resource = resource;
		this->~Entity();
		old_resource->deallocate(this, sizeof(Entity), alignof(Entity));
	}

	Entity::Entity(std::pmr::memory_resource* resource)
		: resource(resource)
	{
		
	}

}