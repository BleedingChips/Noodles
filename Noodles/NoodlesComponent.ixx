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
		Free,
		PreInit,
		Normal,
		Destroy,
		PendingDestroy
	};

	struct Entity : public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<Entity>;
		

	protected:

		void SetFree();

		Entity(Potato::IR::MemoryResourceRecord record);
		static Ptr Create(std::pmr::memory_resource* resource = std::pmr::get_default_resource());
		decltype(auto) GetResource() const { return record.GetResource(); }

		virtual void Release() override;

		Potato::IR::MemoryResourceRecord record;
		mutable std::shared_mutex mutex;
		EntityStatus status = EntityStatus::Free;
		Archetype::Ptr archetype;
		ArchetypeMountPoint mount_point = {};
		std::size_t owner_id = 0;
		std::size_t archetype_index = std::numeric_limits<std::size_t>::max();

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

		ComponentFilterInterface(std::pmr::memory_resource* resource = std::pmr::get_default_resource())
			: indexs(resource) {}

		mutable std::mutex filter_mutex;
		std::pmr::vector<std::size_t> indexs;

		virtual std::span<UniqueTypeID const> GetArchetypeIndex() const = 0;
		virtual void OnCreatedArchetype(std::size_t archetype_index, Archetype const& archetype);

		virtual void AddFilterRef() const = 0;
		virtual void SubFilterRef() const = 0;

		friend struct ArchetypeComponentManager;
	};

	struct SingletonInterface
	{
		struct Wrapper
		{
			template<typename T>
			void AddRef(T* ref) { ref->AddSingletonRef(); }
			template<typename T>
			void SubRef(T* ref) { ref->SubSingletonRef(); }
		};

		using Ptr = Potato::Pointer::IntrusivePtr<SingletonInterface, Wrapper>;

		virtual void* Get() = 0;
		virtual void const* Get() const = 0;
		virtual ~SingletonInterface() = default;

	protected:

		virtual void AddSingletonRef() const = 0;
		virtual void SubSingletonRef() const = 0;

	};

	template<typename Type>
	struct SingletonType : public SingletonInterface 
	{

		template<typename ...OT>
		SingletonType(Potato::IR::MemoryResourceRecord record, OT&& ...ot)
			: record(record), Data(std::forward<OT>(ot)...) {}

		Potato::IR::MemoryResourceRecord record;
		Type Data;

		virtual void* Get() { return &Data; }
		virtual void const* Get() const { return &Data; }
		virtual void AddSingletonRef() const {}
		virtual void SubSingletonRef() const { auto re = record; this->~SingletonType(); re.Deallocate(); }

	};

	struct SingletonFilterInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<SingletonFilterInterface, ComponentFilterInterface::Wrapper>;

		virtual UniqueTypeID RequireTypeID() const = 0;

	protected:

		friend struct ComponentFilterInterface::Wrapper;
		friend struct ArchetypeComponentManager;

		virtual void AddFilterRef() const = 0;
		virtual void SubFilterRef() const = 0;

	private:

		std::mutex mutex;
		std::size_t singleton_reference = std::numeric_limits<std::size_t>::max();
		std::size_t owner_id = 0;
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

		template<typename ...AT>
		EntityPtr CreateEntityDefer(AT&& ...at)
		{
			return CreateEntityDeferWithMemoryResource(std::pmr::get_default_resource(), std::forward<AT>(at)...);
		}

		template<typename ...AT>
		EntityPtr CreateEntityDeferWithMemoryResource(std::pmr::memory_resource* resource, AT&& ...at)
		{

			static_assert(!Potato::TMP::IsRepeat<EntityProperty, std::remove_cvref_t<AT>...>::Value, "Archetype require no repeat component type");

			std::array<ArchetypeID, sizeof...(at) + 1> archetype_ids{
				ArchetypeID::Create<EntityProperty>(),
				ArchetypeID::Create<std::remove_cvref_t<AT>>()...
			};

			std::array<std::size_t, sizeof...(at) + 1> output_index;
			auto [archetype_ptr, mp, archetype_index] = CreateArchetype(archetype_ids, output_index);
			if(archetype_ptr && mp)
			{
				assert(resource != nullptr);
				EntityPtr entity_ptr = Entity::Create(resource);
				if (entity_ptr)
				{
					EntityProperty pro{ entity_ptr };
					archetype_ptr->MoveConstruct(output_index[0], archetype_ptr->GetData(output_index[0], mp), &pro);
					try
					{
						ArchetypeComponentManagerConstructHelper(*archetype_ptr, mp, std::span(output_index).subspan(1), std::forward<AT>(at)...);
						entity_ptr->archetype = archetype_ptr;
						entity_ptr->archetype_index = archetype_index;
						entity_ptr->mount_point = mp;
						entity_ptr->owner_id = reinterpret_cast<std::size_t>(this);
						entity_ptr->status = EntityStatus::PreInit;
						std::lock_guard lg(spawn_mutex);
						spawned_entities.emplace_back(entity_ptr, SpawnedStatus::New);
						need_update = true;
						return entity_ptr;
					}
					catch (...)
					{
						archetype_ptr->Destruction(output_index[0], archetype_ptr->GetData(output_index[0], mp));
						temp_resource.deallocate(mp.GetBuffer(0), archetype_ptr->GetSingleLayout().Size);
						throw;
					}
				}
			}
			return {};
		}

		bool ForceUpdateState();

		bool DestroyEntity(Entity& entity);
		bool RegisterComponentFilter(ComponentFilterInterface::Ptr ptr, std::size_t group_id);
		bool RegisterSingletonFilter(SingletonFilterInterface::Ptr ptr, std::size_t group_id);
		std::size_t ErasesComponentFilter(std::size_t group_id);
		std::size_t ArchetypeCount() const;

		template<typename Func>
		bool ReadyEntity(Entity const& entity, ComponentFilterInterface const& interface, std::size_t count, std::span<std::size_t> output_index, Func&& func)
			requires(std::is_invocable_v<Func, Archetype const&, ArchetypeMountPoint, std::span<std::size_t>>)
		{
			std::lock_guard lg(components_mutex);
			auto [archetype, mp] = ReadEntityImp(entity, interface, count, output_index);
			if(archetype)
			{
				std::forward<Func>(func)(*archetype, mp, output_index.subspan(0, count));
				return true;
			}
			return false;
		}

		template<typename SingType, typename ...OT>
		SingType* CreateSingletonType(OT&& ...ot)
		{
			using Type = SingletonType<std::remove_cvref_t<SingType>>;

			auto ID = UniqueTypeID::Create<std::remove_cvref<SingType>>();

			std::lock_guard lg(pre_init_singletons_mutex);
			auto f = std::find(exist_singleton_id.begin(), exist_singleton_id.end(), ID);
			if(f == exist_singleton_id.end())
			{
				auto re = Potato::IR::MemoryResourceRecord::Allocate<Type>(&singleton_resource);
				if(re)
				{
					Type* ptr = new (re.Get()) Type {re, std::forward<OT>(ot)...};
					pre_init_singletons.emplace_back(ptr, ID);
					exist_singleton_id.push_back(ID);
					return &ptr->Data;
				}
			}
			return nullptr;
		}

		bool ReleaseEntity(Entity::Ptr storage);

	protected:

		std::tuple<Archetype::Ptr, ArchetypeMountPoint> ReadEntityImp(Entity const& entity, ComponentFilterInterface const& interface, std::size_t count, std::span<std::size_t> output_index);

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

		ArchetypeMountPoint AllocateAndConstructMountPoint(Element& tar, ArchetypeMountPoint mp);
		void CopyMountPointFormLast(Element& tar, ArchetypeMountPoint mp);

		enum class SpawnedStatus
		{
			New,
			NewButNeedRemove,
			RemoveOld
		};

		struct SpawnedElement
		{
			EntityPtr entity;
			SpawnedStatus status;
		};

		std::mutex archetype_mutex;
		std::pmr::vector<Archetype::Ptr> new_archetype;
		std::pmr::unsynchronized_pool_resource archetype_resource;

		std::mutex spawn_mutex;
		std::pmr::vector<SpawnedElement> spawned_entities;
		bool need_update = false;
		

		struct CompFilterElement
		{
			ComponentFilterInterface::Ptr filter;
			std::size_t group_id;
		};

		struct SingletonFilterInterfaceElement
		{
			SingletonFilterInterface::Ptr ptr;
			std::size_t group_id;
		};

		std::shared_mutex filter_mapping_mutex;
		std::pmr::vector<CompFilterElement> filter_mapping;
		std::pmr::vector<SingletonFilterInterfaceElement> singleton_filters;

		struct SingletonElement
		{
			SingletonInterface::Ptr single;
			UniqueTypeID id;
		};

		std::shared_mutex singletons_mutex;
		std::pmr::vector<SingletonElement> singletons;

		std::mutex pre_init_singletons_mutex;
		std::pmr::vector<SingletonElement> pre_init_singletons;
		std::pmr::vector<UniqueTypeID> exist_singleton_id;
		std::pmr::unsynchronized_pool_resource singleton_resource;

		std::pmr::synchronized_pool_resource temp_resource;

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