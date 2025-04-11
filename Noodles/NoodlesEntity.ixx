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
import NoodlesGlobalContext;
import NoodlesComponent;

export namespace Noodles
{

	export struct EntityManager;
	export struct EntityProperty;
	

	struct Entity : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<Entity>;

		enum class State
		{
			Free,
			PreInit,
			Normal,
			PendingDestroy,
		};

		~Entity() = default;

	protected:

		static Ptr Create(GlobalContext const& global_context, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		void SetFree_AssumedLocked();

		Entity(
			Potato::IR::MemoryResourceRecord record,
			BitFlagContainer component_bitflag,
			BitFlagContainer modify_component_bitflag
		) : MemoryResourceRecordIntrusiveInterface(record),
			component_bitflag(component_bitflag),
			modify_component_bitflag(modify_component_bitflag)
		{
		}

		mutable std::shared_mutex mutex;
		State state = State::PreInit;

		OptionalSizeT chunk_infos_index;
		std::size_t chunk_index = 0;
		std::size_t component_index = 0;

		BitFlagContainer component_bitflag;
		BitFlagContainer modify_component_bitflag;


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
		struct Config
		{
			std::pmr::memory_resource* resource = std::pmr::get_default_resource();
		};

		EntityManager(GlobalContext& global_context, Config fing = {});
		~EntityManager();

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

		bool RemoveEntityComponent(Entity& target_entity, StructLayout const& struct_layout) { return RemoveEntityComponentImp(target_entity, struct_layout, false); }

		//bool Flush(ComponentManager& manager, std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());

		//bool ReadEntityComponents_AssumedLocked(ComponentManager const& manager, Entity const& ent, ComponentQuery const& filter, QueryData& accessor) const;
		
		~EntityManager();
		//MarkIndex GetEntityPropertyAtomicTypeID() { return entity_entity_property_index; }
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
			BitFlag bitflag;
			StructLayout::Ptr struct_layout;
			Potato::IR::MemoryResourceRecord resource;
			bool Release();
		};

		BitFlag const entity_property_bitflag;
		GlobalContext::Ptr global_context;

		std::mutex entity_mutex;
		std::pmr::vector<EntityModifierInfo> entity_modifier;
		std::pmr::vector<EntityModifierEvent> entity_modifier_event;
	};


	/*
	
	*/
}