module;
#include <cassert>

module NoodlesEntity;

namespace Noodles
{
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


}