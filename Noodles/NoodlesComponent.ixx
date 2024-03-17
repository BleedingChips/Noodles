module;

#include <cassert>

export module NoodlesComponent;

import std;
import PotatoTMP;
import PotatoMisc;
import PotatoPointer;
import PotatoIR;
import PotatoTaskSystem;

import NoodlesMemory;
export import NoodlesArchetype;
export import NoodlesEntity;

export namespace Noodles
{

	export struct ArchetypeComponentManager;



	struct ComponentPage : public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<ComponentPage>;

		static auto Create(
			Potato::IR::Layout component_layout, std::size_t min_element_count, 
			std::size_t min_page_size, std::pmr::memory_resource* up_stream
		) -> Ptr;

		

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

		virtual void Release() override;

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

		Entity(Archetype::Ptr archetype, ArchetypeMountPoint mount_point, Potato::IR::MemoryResourceRecord record);
		static Ptr Create(Archetype::Ptr archetype, ArchetypeMountPoint mount_point, std::pmr::memory_resource* resource = std::pmr::get_default_resource());
		decltype(auto) GetResource() const { return record.GetResource(); }

		virtual void Release() override;

		Potato::IR::MemoryResourceRecord record;
		mutable std::shared_mutex mutex;
		EntityStatus status = EntityStatus::PreInit;
		Archetype::Ptr archetype;
		std::size_t archetype_index = std::numeric_limits<std::size_t>::max();
		ArchetypeMountPoint mount_point;

		friend struct ArchetypeComponentManager;
	};

	using EntityPtr = Entity::Ptr;


	struct EntityProperty
	{
		EntityPtr GetEntity() const { return entity; }

		EntityProperty(EntityProperty const&) = default;
		EntityProperty& operator=(EntityProperty const&) = default;
		EntityProperty(EntityProperty&&) = default;
		EntityProperty(Entity::Ptr entity) : entity(std::move(entity)) {}
		EntityProperty() = default;

		using NoodlesThreadSafeMarker = void;

	protected:

		Entity::Ptr entity;

		friend struct ArchetypeComponentManager;
	};

	/*
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
	*/

	struct ArchetypeMountPointRange
	{
		Archetype const& archetype;

		ArchetypeMountPoint begin() const { return mp_begin; }
		ArchetypeMountPoint end() const { return mp_end; }

		ArchetypeMountPoint mp_begin;
		ArchetypeMountPoint mp_end;
	};

	struct ArchetypeComponentFilter
	{

		struct Wrapper
		{
			
		};

		using Ptr = Potato::Pointer::IntrusivePtr<ArchetypeComponentFilter>;

		virtual ~ArchetypeComponentFilter() = default;

	protected:

		virtual void CollectArchetype(Archetype const& archetype);

	};

	struct ComponentFilterInterface
	{

		struct Wrapper
		{
			template<typename T>
			void AddRef(T* ref) { ref->AddFilterRef(); }
			template<typename T>
			void SubRef(T* ref) { ref->SubFilterRef();  }
		};

		using Ptr = Potato::Pointer::IntrusivePtr<ComponentFilterInterface, Wrapper>;

		virtual ~ComponentFilterInterface() = default;

	protected:

		ComponentFilterInterface(std::pmr::memory_resource* resource)
			: indexs(resource) {}

		std::mutex filter_mutex;
		std::pmr::vector<std::size_t> indexs;

		virtual std::span<UniqueTypeID> GetArchetypeIndex() const = 0;
		virtual void OnCreatedArchetype(std::size_t archetype_index, Archetype const& archetype);


		//virtual bool Collect(std::size_t archetype_index, Archetype const& archetype) = 0;


		virtual void AddFilterRef() const = 0;
		virtual void SubFilterRef() const = 0;

		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
		friend struct ArchetypeComponentManager;
	};

	inline void ArchetypeComponentManagerConstructHelper(Archetype const& ac, ArchetypeMountPoint mp, std::span<std::size_t> index){}

	template<typename T, typename ...AT>
	inline void ArchetypeComponentManagerConstructHelper(Archetype const& ac, ArchetypeMountPoint mp, std::span<std::size_t> index, T&& ref, AT&& ...at)
	{
		std::remove_cvref_t<T> tem_ref{std::forward<T>(ref)};
		ac.MoveConstruct(index[0], ac.GetData(index[0], mp), &tem_ref);
		try
		{
			ArchetypeComponentManagerConstructHelper(ac, mp, index.subspan(1), std::forward<AT>(at)...);
		}catch (...)
		{
			ac.Destruction(index[0], ac.GetData(index[0], mp));
			throw;
		}
	};

	export struct ArchetypeComponentManager
	{

		ArchetypeComponentManager(std::pmr::memory_resource* upstream = std::pmr::get_default_resource());
		~ArchetypeComponentManager();

		/*
		template<typename Func>
		EntityPtr CreateEntityDeferFromFunction(std::span<ArchetypeID const> IDs, Func&& func) requires(std::is_invocable_v<Func, std::span<void*>>)
		{
			return CreateEntityDeferImp(IDs, [](void* obj, std::span<void*> buffer)
			{
				(*static_cast<Func*>(obj))(buffer);
			}, &func);
		}
		*/

		template<typename ...AT>
		EntityPtr CreateEntityDefer(AT&& ...at)
		{

			static_assert(!Potato::TMP::IsRepeat<EntityProperty, std::remove_cvref_t<AT>...>::Value, "Archetype require no repeat component type");

			std::array<ArchetypeID, sizeof...(at) + 1> archetype_ids{
				ArchetypeID::Create<EntityProperty>(),
				ArchetypeID::Create<std::remove_cvref_t<AT>>()...
			};

			std::array<std::size_t, sizeof...(at) + 1> output_index;
			auto [archetype_ptr, mp, archetype_index] = CreateArchetype(archetype_ids, output_index);
			if(archetype_ptr)
			{
				assert(entity_resource);
				EntityPtr entity_ptr = Entity::Create(archetype_ptr, mp, entity_resource->get_resource_interface());
				EntityProperty pro{entity_ptr};
				archetype_ptr->MoveConstruct(output_index[0], archetype_ptr->GetData(output_index[0], mp), &pro);
				try
				{
					ArchetypeComponentManagerConstructHelper(*archetype_ptr, mp, std::span(output_index).subspan(1), std::forward<AT>(at)...);
				}catch(...)
				{
					archetype_ptr->Destruction(output_index[0], archetype_ptr->GetData(output_index[0], mp));
					std::lock_guard lg(archetype_mutex);
					temp_archetype_resource.deallocate(mp.GetBuffer(0), archetype_ptr->GetSingleLayout().Size);
					throw;
				}
				
				std::lock_guard lg(spawn_mutex);
				spawned_entities.emplace_back(entity_ptr, mp);
				return entity_ptr;
			}
			return {};
		}

		bool UpdateEntityStatus();
		bool DestroyEntity(Entity& entity);
		bool RegisterComponentFilter(ComponentFilterInterface::Ptr ptr, std::size_t group_id);
		std::size_t ErasesComponentFilter(std::size_t group_id);
		std::size_t ArchetypeCount() const;

	protected:

		std::tuple<Archetype::Ptr, ArchetypeMountPoint, std::size_t> CreateArchetype(std::span<ArchetypeID const> ids, std::span<std::size_t> output);
		static bool CheckIsSameArchetype(Archetype const& target, std::size_t hash_code, std::span<ArchetypeID const> ids, std::span<std::size_t> output);


		static inline void ComponentConstructorHelper(std::span<void*> input)
		{
			assert(input.size() == 0);
		}

		template<typename A, typename ...AT>
		static inline void ComponentConstructorHelper(std::span<void*> input, A&& a, AT&& ...at)
		{
			assert(input.size() >= 1);
			new (input[0]) std::remove_cvref_t<A>{std::forward<A>(a)};
			ComponentConstructorHelper(input.subspan(1), std::forward<AT>(at)...);
		}

		static void ReleaseEntity(Entity& storage);

		struct Element
		{
			Archetype::Ptr archetype;
			ComponentPage::Ptr top_page;
			ComponentPage::Ptr last_page;
			std::size_t total_count;
		};

		mutable std::shared_mutex components_mutex;
		std::pmr::vector<Element> components;
		std::pmr::synchronized_pool_resource components_resource;

		struct SpawnedElement
		{
			EntityPtr entity;
			ArchetypeMountPoint mp;
		};

		std::mutex archetype_mutex;
		std::size_t startup_index = 0;
		std::pmr::vector<Archetype::Ptr> new_archetype;
		std::pmr::monotonic_buffer_resource archetype_resource;
		std::pmr::monotonic_buffer_resource temp_archetype_resource;

		std::mutex spawn_mutex;
		std::pmr::vector<SpawnedElement> spawned_entities;
		
		bool need_update = false;

		struct RemoveEntity
		{
			Archetype::Ptr archetype;
			std::size_t fast_index = std::numeric_limits<std::size_t>::max();;
			ArchetypeMountPoint mount_point;
			EntityStatus last_status;
		};

		std::pmr::vector<RemoveEntity> removed_entities;
		

		struct CompFilterElement
		{
			ComponentFilterInterface::Ptr filter;
			std::size_t group_id;
		};

		std::shared_mutex filter_mapping_mutex;
		std::pmr::vector<CompFilterElement> filter_mapping;

		Memory::IntrusiveMemoryResource<std::pmr::synchronized_pool_resource>::Ptr entity_resource;

		friend struct EntityConstructor;
		friend struct ComponentFilterInterface;

		bool ForeachMountPoint(std::size_t element_index, bool(*func)(void*, ArchetypeMountPointRange), void* data) const;
		bool ForeachMountPoint(std::size_t element_index, bool(*detect)(void*, Archetype const&), void* data, bool(*func)(void*, ArchetypeMountPointRange), void* data2) const;
		bool ReadEntityMountPoint(Entity const& storage, void(*func)(void*, EntityStatus, Archetype const&, ArchetypeMountPoint), void* data) const;

	public:

		struct ComponentWrapper
		{
			std::size_t element_count;
			std::span<std::size_t> reference_index;
		};

		/*
		template<typename Func>
		bool ReadComponent(ComponentFilterInterface& interface, Func&& func) const requires(std::is_invocable_v<Func&&, ComponentWrapper&>)
		{
			std::shared_lock sl(components_mutex);
			ComponentWrapper wrapper(interface.GetArchetypeIndex().size(), std::span(interface.indexs));
			func(wrapper);
		}

		struct EntityWrapper
		{
			std::span<std::size_t> reference_index;
		};

		template<typename Func>
		bool ReadEntity(Entity const& entity, ComponentFilterInterface& interface, Func&& func, std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource()) const requires(std::is_invocable_v<Func&&, ComponentWrapper&>)
		{
			std::shared_lock sl(components_mutex);
			std::shared_lock el(entity.mutex);
			auto EC = interface.GetArchetypeIndex().size();
			assert((interface.indexs.size() % EC) == 0);
			if(entity.archetype && entity.GetResource() == entity_resource->get_resource_interface())
			{
				std::pmr::vector<std::size_t> temp_v{ temp_resource };
				std::span<std::size_t> ref = std::span(interface.indexs);
				if(entity.archetype_index != std::numeric_limits<std::size_t>::max())
				{
					while(!ref.empty())
					{
						if(ref[0] == entity.archetype_index)
						{
							ref = ref.subspan(0, EC);
							break;
						}else
						{
							ref = ref.subspan(EC);
						}
					}
					if(ref.empty())
					{
						return false;
					}
				}else
				{
					
				}
			}
			return false;

			if(entity.status == EntityStatus::Normal || entity.status == )
			{
				assert();
			}

			ComponentWrapper wrapper(interface.GetArchetypeIndex().size(), std::span(interface.indexs));
			func(wrapper);
		}

		struct ComponentIterator
		{
			ArchetypeComponentManager& manager;
			std::span<std::size_t> output;
		};

		bool ReadMountPointRange(ComponentFilterInterface& interface, std::size_t ite_index, std::span<void*> output_buffer);

		template<typename Func>
		bool ReadMountPoint(std::size_t archetype_index, Func&& fun) const
			requires(std::is_invocable_v<Func, ComponentPage::Ptr, ComponentPage::Ptr>)
		{
			std::shared_lock lg(components_mutex);
			if(components.size() > archetype_index)
			{
				auto& ref = components[archetype_index];
				std::forward<Func>(fun)(ref.top_page, ref.last_page);
				return true;
			}
			return false;
		}

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
		*/
	};

}