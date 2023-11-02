module;

export module NoodlesEntity;

import std;
import PotatoMisc;
import PotatoPointer;

import NoodlesMemory;
import NoodlesArchetype;

export namespace Noodles
{
	struct ArchetypeComponentManager;
}

export namespace Noodles
{

	struct EntityStorage : public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<EntityStorage>;

	protected:

		EntityStorage(std::pmr::memory_resource* Resource);

		static Ptr Create(std::pmr::memory_resource* Resource = std::pmr::get_default_resource());
		virtual void Release() override;

		std::pmr::memory_resource* resource = nullptr;
		std::shared_mutex mutex;
		Archetype::Ptr archetype;
		ArchetypeMountPoint mount_point;

		friend struct ArchetypeComponentManager;
	};

	using Entity = EntityStorage::Ptr;

}