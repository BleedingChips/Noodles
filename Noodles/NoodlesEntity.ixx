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
export import NoodlesQuery;

export namespace Noodles
{

	export struct EntityManager;
	export struct EntityProperty;
	

	struct Entity : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{

		using Ptr = Potato::Pointer::IntrusivePtr<Entity>;

		static Ptr Create(StructLayoutManager const& layout_manager, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		enum class State
		{
			Free,
			PreInit,
			Normal,
			PendingDestroy,
		};

		~Entity() = default;

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

		EntityProperty(EntityProperty &&) = default;
		//EntityProperty& operator=(EntityProperty const&) = default;
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

		
		Entity::Ptr CreateEntity(std::pmr::memory_resource* entity_resource = std::pmr::get_default_resource(), std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());

		enum class Operation
		{
			Move,
			Copy
		};


		template<typename Type>
		bool AddEntityComponent(Entity& target_entity, Type&& type, std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource()) requires(std::is_rvalue_reference_v<decltype(type)>)
		{
			return this->AddEntityComponent(target_entity, *StructLayout::GetStatic<Type>(), &type, std::is_rvalue_reference_v<Type&&> ? Operation::Move : Operation::Copy, temp_resource);
		}

		bool AddEntityComponent(Entity& target_entity, StructLayout const& struct_layout, void* reference_buffer, Operation operation = Operation::Copy, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
		{
			return AddEntityComponentImp(target_entity, struct_layout, reference_buffer, false, operation, resource);
		}

		template<typename Type>
		bool RemoveEntityComponent(Entity& target_entity) { return this->RemoveEntityComponent(std::move(target_entity), *StructLayout::GetStatic<Type>()); }

		bool RemoveEntityComponent(Entity& target_entity, StructLayout const& struct_layout){ return RemoveEntityComponentImp(target_entity, struct_layout, false); }

		bool Flush(ComponentManager& manager, std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());

		bool ReadEntityComponents_AssumedLocked(ComponentManager const& manager, Entity const& ent, ComponentQuery const& filter, QueryData& accessor) const;
		EntityManager(StructLayoutManager& manager, Config fing = {});
		~EntityManager();
		MarkIndex GetEntityPropertyAtomicTypeID() { return entity_entity_property_index; }
		bool ReleaseEntity(Entity::Ptr entity);

	protected:

		bool AddEntityComponentImp(Entity& target_entity, StructLayout const& struct_layout, void* reference_buffer, bool accept_build_in, Operation operation, std::pmr::memory_resource* resource = std::pmr::get_default_resource());
		bool RemoveEntityComponentImp(Entity& target_entity, StructLayout const& struct_layout, bool accept_build_in_component = false);

		
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

		MarkIndex const entity_entity_property_index;
		StructLayoutManager::Ptr manager;

		std::mutex entity_mutex;
		std::pmr::vector<EntityModifierInfo> entity_modifier;
		std::pmr::vector<EntityModifierEvent> entity_modifier_event;
	};

}