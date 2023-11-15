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

	enum class EntityStatus
	{
		PreInit,
		Normal,
		Destroy,
		PendingDestroy
	};

	struct Entity : public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<Entity>;

	protected:

		Entity(std::pmr::memory_resource* Resource);

		static Ptr Create(std::pmr::memory_resource* Resource = std::pmr::get_default_resource());
		virtual void Release() override;

		std::pmr::memory_resource* resource = nullptr;
		mutable std::shared_mutex mutex;
		EntityStatus status = EntityStatus::PreInit;
		Archetype::Ptr archetype;
		ArchetypeMountPoint mount_point;

		friend struct ArchetypeComponentManager;
	}; 

	using EntityPtr = Entity::Ptr;

}