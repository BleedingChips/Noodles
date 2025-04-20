module;

#include <cassert>

export module NoodlesEntity;

import std;
import PotatoTMP;
import PotatoPointer;
import PotatoIR;
import PotatoMisc;

import NoodlesMisc;
import NoodlesBitFlag;
import NoodlesClassBitFlag;
import NoodlesComponent;

export namespace Noodles
{

	export struct EntityManager;
	export struct EntityProperty;
	

	struct Entity : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<Entity>;
		using Index = ComponentManager::Index;

		enum class State
		{
			Free,
			PreInit,
			Normal,
			PendingDestroy,
		};

		~Entity() = default;

		std::optional<Index> GetEntityIndex() const { std::shared_lock sl(mutex);  return index; }

	protected:

		static Ptr Create(std::size_t component_bitflag_container_count, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		void SetFree_AssumedLocked();

		Entity(
			Potato::IR::MemoryResourceRecord record,
			BitFlagContainerViewer component_bitflag,
			BitFlagContainerViewer modify_component_bitflag
		) : MemoryResourceRecordIntrusiveInterface(record),
			component_bitflag(component_bitflag),
			modify_component_bitflag(modify_component_bitflag)
		{
		}

		mutable std::shared_mutex mutex;
		State state = State::PreInit;

		std::optional<Index> index;

		OptionalSizeT modify_index;

		BitFlagContainerViewer component_bitflag;
		BitFlagContainerViewer modify_component_bitflag;


		friend struct EntityManager;
		friend struct EntityProperty;

		friend struct Ptr::CurrentWrapper;
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

		friend struct EntityManager;
	};

	export struct EntityManager
	{

		using Index = Entity::Index;

		struct Config
		{
			std::pmr::memory_resource* resource = std::pmr::get_default_resource();
		};

		EntityManager(AsynClassBitFlagMap& mapping, Config config = {});
		~EntityManager();

		Entity::Ptr CreateEntity(std::pmr::memory_resource* entity_resource = std::pmr::get_default_resource(), std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());

		enum class Operation
		{
			Move,
			Copy
		};

		template<typename Type>
		bool AddEntityComponent(Entity& entity, Type&& component, BitFlag component_bitflag,std::pmr::memory_resource* entity_resource = std::pmr::get_default_resource()) requires(std::is_rvalue_reference_v<decltype(component)>)
		{
			return this->AddEntityComponent(entity, *StructLayout::GetStatic<Type>(), &component, component_bitflag, std::is_rvalue_reference_v<Type&&> ? Operation::Move : Operation::Copy, entity_resource);
		}

		bool AddEntityComponent(Entity& entity, StructLayout const& component_class, void* component_ptr, BitFlag component_bitflag, Operation operation = Operation::Copy, std::pmr::memory_resource* entity_resource = std::pmr::get_default_resource())
		{
			return AddEntityComponentImp(entity, component_class, component_ptr, component_bitflag, false, operation, entity_resource);
		}

		bool RemoveEntityComponent(Entity& entity, BitFlag component_bitflag) { return RemoveEntityComponentImp(entity, component_bitflag, false); }

		bool FlushEntityModify(ComponentManager& manager, std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());

		//bool ReadEntityComponents_AssumedLocked(ComponentManager const& manager, Entity const& ent, ComponentQuery const& filter, QueryData& accessor) const;

		BitFlag GetEntityPropertyBitFlag() { return entity_property_bitflag; }
		bool ReleaseEntity(Entity& entity);

	protected:

		bool AddEntityComponentImp(Entity& entity, StructLayout const& component_class, void* component_ptr, BitFlag const component_bitflag, bool accept_build_in, Operation operation, std::pmr::memory_resource* resource = std::pmr::get_default_resource());
		bool RemoveEntityComponentImp(Entity& entity, BitFlag const component_bitflag, bool accept_build_in_component = false);


		struct EntityModifierInfo
		{
			bool need_remove = false;
			Potato::Misc::IndexSpan<> infos;
			Entity::Ptr entity;
		};

		struct EntityModifierEvent
		{
			bool need_add = false;
			BitFlag bitflag;
			StructLayout::Ptr struct_layout;
			Potato::IR::MemoryResourceRecord resource;
			bool Release();
		};

		BitFlag entity_property_bitflag;
		std::size_t componenot_bitflag_container_count;

		std::pmr::vector<EntityModifierInfo> entity_modifier;
		std::pmr::vector<EntityModifierEvent> entity_modifier_event;
	};


	/*
	
	*/
}