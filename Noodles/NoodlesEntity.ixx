module;

#include <cassert>

export module NoodlesEntity;

import std;
import PotatoTMP;
import PotatoPointer;
import PotatoIR;
import PotatoMisc;

export import NoodlesArchetype;
export import NoodlesComponent;

export namespace Noodles
{

	export struct EntityManager;
	export struct EntityProperty;
	

	struct Entity : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{

		using Ptr = Potato::Pointer::IntrusivePtr<Entity>;

		static Ptr Create(ComponentManager const& component_manager, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		enum class State
		{
			Free,
			PreInit,
			Normal,
			PendingDestroy,
		};

	protected:

		void SetFree_AssumedLocked();

		Entity(
			Potato::IR::MemoryResourceRecord record,
			std::span<MarkElement> current_component_mask,
			std::span<MarkElement> modify_component_mask
		) : MemoryResourceRecordIntrusiveInterface(record),
			current_component_mask(current_component_mask),
			modify_component_mask(modify_component_mask)
		{
		}

		~Entity();


		Potato::IR::MemoryResourceRecord record;
		mutable std::shared_mutex mutex;

		State state = State::PreInit;

		OptionalIndex archetype_index;
		std::size_t column_index;
		OptionalIndex modify_index;

		std::span<MarkElement> current_component_mask;
		std::span<MarkElement> modify_component_mask;


		friend struct EntityManager;
		friend struct EntityProperty;

		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
	};

	export struct EntityProperty
	{
		Entity::Ptr GetEntity() const { return entity; }

		EntityProperty(EntityProperty const&) = default;
		EntityProperty& operator=(EntityProperty const&) = default;
		EntityProperty(EntityProperty&&) = default;
		EntityProperty(Entity::Ptr entity) : entity(std::move(entity)) {}
		EntityProperty() = default;
		~EntityProperty();

	protected:

		Entity::Ptr entity;

		friend struct ArchetypeComponentManager;
	};

	export struct EntityManager
	{
		struct Config
		{
			std::pmr::memory_resource* resource = std::pmr::get_default_resource();
		};

		
		Entity::Ptr CreateEntity(ComponentManager& manager, std::pmr::memory_resource* entity_resource = std::pmr::get_default_resource(), std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());

		enum class Operation
		{
			Move,
			Copy
		};


		template<typename Type>
		bool AddEntityComponent(ComponentManager& manager, Entity& target_entity, Type&& type, std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource()) requires(std::is_rvalue_reference_v<decltype(type)>)
		{
			return this->AddEntityComponent(manager, target_entity, *StructLayout::GetStatic<Type>(), &type, std::is_rvalue_reference_v<Type&&> ? Operation::Move : Operation::Copy, temp_resource);
		}

		bool AddEntityComponent(ComponentManager& manager, Entity& target_entity, StructLayout const& struct_layout, void* reference_buffer, Operation operation = Operation::Copy, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
		{
			return AddEntityComponentImp(manager, target_entity, struct_layout, reference_buffer, false, operation, resource);
		}

		template<typename Type>
		bool RemoveEntityComponent(ComponentManager& manager, Entity& target_entity) { return this->RemoveEntityComponent(manager, std::move(target_entity), *StructLayout::GetStatic<Type>()); }

		bool RemoveEntityComponent(ComponentManager& manager, Entity& target_entity, StructLayout const& struct_layout){ return RemoveEntityComponentImp(manager, target_entity, struct_layout, false); }

		bool Flush(ComponentManager& manager, std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());

		std::optional<ComponentRowWrapper> ReadEntityComponents_AssumedLocked(ComponentManager const& manager, Entity const& ent, ComponentFilter const& filter, std::pmr::memory_resource* wrapper_resource = std::pmr::get_default_resource()) const;

		EntityManager(Config fing = {});
		~EntityManager();
		bool Init(ComponentManager& manager);
		static MarkIndex GetEntityPropertyAtomicTypeID() { return MarkIndex{ 0 }; }
		bool ReleaseEntity(Entity::Ptr entity);

	protected:

		bool AddEntityComponentImp(ComponentManager& manager, Entity& target_entity, StructLayout const& struct_layout, void* reference_buffer, bool accept_build_in, Operation operation, std::pmr::memory_resource* resource = std::pmr::get_default_resource());
		bool RemoveEntityComponentImp(ComponentManager& manager, Entity& target_entity, StructLayout const& struct_layout, bool accept_build_in_component = false);

		
		struct EntityModifierInfo
		{
			bool need_remove = false;
			Potato::Misc::IndexSpan<> infos;
			Entity::Ptr entity;
		};

		struct EntityModifierEvent
		{
			bool need_add = false;
			MarkIndex index;
			StructLayout::Ptr struct_layout;
			Potato::IR::MemoryResourceRecord resource;
			bool Release();
		};

		std::mutex entity_mutex;
		std::pmr::vector<EntityModifierInfo> entity_modifier;
		std::pmr::vector<EntityModifierEvent> entity_modifier_event;
	};

}