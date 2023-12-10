module;

#include <cassert>

export module NoodlesComponent;

import std;
import PotatoMisc;
import PotatoPointer;
import PotatoIR;
import PotatoTaskSystem;

import NoodlesMemory;
export import NoodlesArchetype;
export import NoodlesEntity;

export namespace Noodles
{

	struct ComponentPage : public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<ComponentPage>;

		static auto Create(
			Potato::IR::Layout component_layout, std::size_t min_element_count, 
			std::size_t min_page_size, std::pmr::memory_resource* up_stream
		) -> Ptr;

		virtual void Release() override;

	

		ArchetypeMountPoint begin() const
		{
			return {
				buffer.data(),
				max_element_count,
				0
			};
		}
		ArchetypeMountPoint end() const
		{
			return {
				buffer.data(),
				max_element_count,
				available_count
			};
		}

		ArchetypeMountPoint GetLastMountPoint() const
		{
			return end();
		}

		ArchetypeMountPoint GetMaxMountPoint() const
		{
			return {
				buffer.data(),
				max_element_count,
				max_element_count
			};
		}

		Ptr GetNextPage() const { return next_page; }

	protected:

		ComponentPage(
			std::size_t max_element_count,
			std::size_t allocate_size,
			std::pmr::memory_resource* upstream,
			std::span<std::byte> buffer
		);

		virtual ~ComponentPage() = default;

		Ptr next_page;

		std::size_t const max_element_count = 0;
		std::size_t available_count = 0;
		std::span<std::byte> const buffer;
		std::size_t const allocate_size = 0;
		std::pmr::memory_resource* const resource = nullptr;

		friend struct ArchetypeComponentManager;
	};

	struct EntityFlags
	{
		std::pmr::vector<std::size_t> flags;
	};

	struct EntityProperty
	{
		Entity::Ptr GetEntity() const { return entity; }

		EntityProperty(EntityProperty const&) = default;
		EntityProperty& operator=(EntityProperty const&) = default;
		EntityProperty(EntityProperty&&) = default;
		EntityProperty(Entity::Ptr entity) : entity(std::move(entity)) {}
		EntityProperty() = default;

		using NoodlesSingletonRequire = void;

	protected:

		

		Entity::Ptr entity;

		friend struct ArchetypeComponentManager;
	};


	struct EntityConstructor
	{
		EntityConstructor(std::pmr::memory_resource* upstream = std::pmr::get_default_resource());
		~EntityConstructor();

		bool MoveConstructRaw(ArchetypeID const& id, void* data);

		template<typename MoveConstructRow>
		bool MoveConstruct(MoveConstructRow&& roa)
		{
			if constexpr (std::is_move_assignable_v<decltype(roa)>)
			{
				return MoveConstructRaw(ArchetypeID::Create<std::remove_cvref_t<MoveConstructRow>>(), &roa);
			}else
			{
				std::remove_cvref_t<MoveConstructRow> tem{ std::forward<MoveConstructRow>(roa) };
				return MoveConstructRaw(ArchetypeID::Create<std::remove_cvref_t<MoveConstructRow>>(), &tem);
			}
		}

	protected:

		std::pmr::monotonic_buffer_resource temp_resource;
		ArchetypeConstructor arc_constructor;

		struct Element
		{
			std::size_t count;
			ArchetypeID id;
			void* buffer;
		};

		std::pmr::vector<Element> elements;

		friend struct ArchetypeComponentManager;
	};

	struct ArchetypeComponentManager;

	struct ArchetypeMountPointRange
	{
		Archetype const& archetype;

		ArchetypeMountPoint begin() const { return mp_begin; }
		ArchetypeMountPoint end() const { return mp_end; }

		ArchetypeMountPoint mp_begin;
		ArchetypeMountPoint mp_end;
	};

	struct ComponentFilterInterface
	{

		using Ptr = Potato::Pointer::IntrusivePtr<ComponentFilterInterface>;

		virtual ~ComponentFilterInterface() = default;

	protected:

		virtual bool TryPreCollection(std::size_t element_index, Archetype const& archetype) = 0;
		virtual void AddRef() const = 0;
		virtual void SubRef() const = 0;

		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
		friend struct ArchetypeComponentManager;
	};

	struct ArchetypeComponentManager
	{

		ArchetypeComponentManager(std::pmr::memory_resource* upstream = std::pmr::get_default_resource());
		~ArchetypeComponentManager();

		EntityPtr CreateEntityDefer(EntityConstructor const& con);

		bool UpdateEntityStatus();
		bool DestroyEntity(Entity& entity);
		bool RegisterComponentFilter(ComponentFilterInterface::Ptr ptr, std::size_t group_id);
		std::size_t ErasesComponentFilter(std::size_t group_id);
		std::size_t ArchetypeCount() const;

	protected:

		static ArchetypeID const& EntityPropertyArchetypeID();

		static void ReleaseEntity(Entity& storage);

		struct Element
		{
			Archetype::Ptr archetype;
			ComponentPage::Ptr top_page;
			ComponentPage::Ptr last_page;
		};

		mutable std::shared_mutex components_mutex;
		std::pmr::vector<Element> components;
		std::pmr::synchronized_pool_resource components_resource;

		std::mutex spawn_mutex;
		std::pmr::vector<EntityPtr> spawned_entities;
		std::pmr::monotonic_buffer_resource spawned_entities_resource;
		bool need_update = false;

		struct RemoveEntity
		{
			Archetype::Ptr arche;
			ArchetypeMountPoint mount_point;
			EntityStatus last_status;
		};

		std::pmr::vector<RemoveEntity> removed_entities;

		std::mutex archetype_resource_mutex;
		std::pmr::monotonic_buffer_resource archetype_resource;

		

		struct CompFilterElement
		{
			ComponentFilterInterface::Ptr filter;
			std::size_t group_id;
		};

		std::mutex filter_mapping_mutex;
		std::pmr::vector<CompFilterElement> filter_mapping;

		Memory::IntrusiveMemoryResource<std::pmr::synchronized_pool_resource>::Ptr entity_resource;

		friend struct EntityConstructor;
		friend struct ComponentFilterInterface;

		bool ForeachMountPoint(std::size_t element_index, bool(*func)(void*, ArchetypeMountPointRange), void* data) const;
		bool ForeachMountPoint(std::size_t element_index, bool(*detect)(void*, Archetype const&), void* data, bool(*func)(void*, ArchetypeMountPointRange), void* data2) const;
		bool ReadEntityMountPoint(Entity const& storage, void(*func)(void*, EntityStatus, Archetype const&, ArchetypeMountPoint), void* data) const;

	public:

		template<typename Func>
		bool ForeachMountPoint(std::size_t element_index, Func&& func) const
			requires(std::is_invocable_r_v<bool, Func, ArchetypeMountPointRange>)
		{
			return ForeachMountPoint(element_index, [](void* data, ArchetypeMountPointRange range)->bool
			{
				return (*static_cast<Func*>(data))(range);
			}, static_cast<void*>(&func));
		}

		template<typename FilterFunc, typename Func>
		bool ForeachMountPoint(std::size_t element_index, FilterFunc&& filter_func, Func&& func) const
			requires(
				std::is_invocable_r_v<bool, FilterFunc, Archetype const&>
				&& std::is_invocable_v<Func, ArchetypeMountPointRange>
				)
		{
			return ForeachMountPoint(element_index, 
				[](void* data, Archetype const& arc)
				{
					(*static_cast<FilterFunc*>(data))(arc);
				}, static_cast<void*>(&filter_func),
				[](void* data, ArchetypeMountPointRange range)->bool
				{
					return (*static_cast<Func*>(data))(range);
				}, &func
				);
		}

		template<typename Func>
		bool ReadEntityMountPoint(Entity const& entity, Func&& func) const
			requires(
				std::is_invocable_v<Func, EntityStatus,  Archetype const&, ArchetypeMountPoint>
			)
		{
			return ReadEntityMountPoint(entity, [](void* data, EntityStatus status, Archetype const& arc, ArchetypeMountPoint mp)
				{
					(*static_cast<Func*>(data))(status, arc, mp);
				}, static_cast<void*>(&func));
		}
	};

}