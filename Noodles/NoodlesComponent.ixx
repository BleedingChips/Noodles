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

	template<bool mutex>
	struct NoodlesTypeProperty
	{
		static constexpr bool ignore_mutex = !mutex;
	};

	using IgnoreMutexProperty = NoodlesTypeProperty<false>;


	export struct ArchetypeComponentManager;

	struct ComponentPage : public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<ComponentPage>;

		static auto Create(
			Potato::IR::Layout component_layout, std::size_t min_element_count, 
			std::size_t min_page_size, std::pmr::memory_resource* up_stream
		) -> Ptr;

		Archetype::ArrayMountPoint GetMountPoint() const
		{
			return {
				buffer.data(),
				available_count,
				max_element_count
			};
		}

	protected:

		virtual void Release() override;

		ComponentPage(
			std::size_t max_element_count,
			std::size_t allocate_size,
			std::pmr::memory_resource* upstream,
			std::span<std::byte> buffer
		);

		virtual ~ComponentPage() = default;

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
		Archetype::OPtr archetype;
		Archetype::ArrayMountPoint mount_point = {};
		std::size_t array_mount_point_index = 0;
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

		using NoodlesProperty = IgnoreMutexProperty;

	protected:

		Entity::Ptr entity;

		friend struct ArchetypeComponentManager;
	};

	

	struct FilterInterface
	{
		struct Wrapper
		{
			template<typename T>
			void AddRef(T* ref) { ref->AddFilterRef(); }
			template<typename T>
			void SubRef(T* ref) { ref->SubFilterRef(); }
		};

		bool Register(std::size_t owner_id);
		bool Unregister(std::size_t owner_id);

	protected:

		virtual void OnUnregister() {};

		virtual void AddFilterRef() const = 0;
		virtual void SubFilterRef() const = 0;

		virtual ~FilterInterface() = default;

		mutable std::mutex filter_mutex;
		std::size_t owner_id = 0;
	};

	struct ComponentFilterInterface : public FilterInterface
	{

		using Ptr = Potato::Pointer::IntrusivePtr<ComponentFilterInterface, Wrapper>;

		virtual ~ComponentFilterInterface() = default;

		std::optional<std::size_t> EnumByArchetypeIndex(std::size_t owner_id, std::size_t archetype_index, std::span<std::size_t> output_index) const;
		std::optional<std::size_t> EnumByIteratorIndex(std::size_t owner_id, std::size_t ite_index, std::size_t& archetype_index, std::span<std::size_t> output_index) const;

	protected:

		virtual void OnCreatedArchetype(std::size_t owner_id, std::size_t archetype_index, Archetype const& archetype);
		virtual void OnUnregister() override;

		ComponentFilterInterface(std::pmr::memory_resource* resource = std::pmr::get_default_resource())
			: indexs(resource) {}

		std::pmr::vector<std::size_t> indexs;

		virtual std::span<UniqueTypeID const> GetArchetypeIndex() const = 0;
		

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
	struct SingletonType : public SingletonInterface, public Potato::Pointer::DefaultIntrusiveInterface
	{
		using PureType = Type;
		template<typename ...OT>
		SingletonType(Potato::IR::MemoryResourceRecord record, OT&& ...ot)
			: record(record), Data(std::forward<OT>(ot)...) {}

		Potato::IR::MemoryResourceRecord record;
		Type Data;

		virtual void* Get() { return &Data; }
		virtual void const* Get() const { return &Data; }

	protected:

		virtual void Release() override
		{
			auto re = record; this->~SingletonType(); re.Deallocate();
		}
		virtual void AddSingletonRef() const { AddRef(); }
		virtual void SubSingletonRef() const { SubRef(); }

	};

	struct SingletonFilterInterface : public FilterInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<SingletonFilterInterface, FilterInterface::Wrapper>;

		virtual UniqueTypeID RequireTypeID() const = 0;

		void* GetSingleton(std::size_t owner_id) const;

	protected:

		virtual void OnUnregister() override;

		friend struct ArchetypeComponentManager;

	private:

		SingletonInterface::Ptr singleton_reference;
	};

	inline void ArchetypeComponentManagerConstructHelper(Archetype const& ac, Archetype::ArrayMountPoint mp, std::size_t mp_index, std::span<std::size_t> index){}

	template<typename T, typename ...AT>
	inline void ArchetypeComponentManagerConstructHelper(Archetype const& ac, Archetype::ArrayMountPoint mp, std::size_t mp_index, std::span<std::size_t> index, T&& ref, AT&& ...at)
	{
		std::remove_cvref_t<T> tem_ref{std::forward<T>(ref)};
		auto& el = ac.GetInfos(index[0]);
		ac.MoveConstruct(el, ac.Get(ac.Get(el, mp), mp_index), &tem_ref);
		try
		{
			ArchetypeComponentManagerConstructHelper(ac, mp, index.subspan(1), std::forward<AT>(at)...);
		}catch (...)
		{
			ac.Destruct(el, ac.Get(ac.Get(el, mp), mp_index));
			throw;
		}
	};

	

	export struct ArchetypeComponentManager
	{
		struct SyncResource
		{
			std::pmr::memory_resource* manager_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* archetype_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* component_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* singleton_resource = std::pmr::get_default_resource();
		};

		ArchetypeComponentManager(SyncResource resource = {});
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
					auto& ref = archetype_ptr->GetInfos(output_index[0]);
					archetype_ptr->MoveConstruct(ref, archetype_ptr->GetData(ref, mp), &pro);
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
						archetype_ptr->Destruct(ref, mp);
						temp_resource.deallocate(mp.GetBuffer(), archetype_ptr->GetSingleLayout().Size);
						throw;
					}
				}
			}
			return {};
		}

		bool ForceUpdateState();

		bool RegisterFilter(ComponentFilterInterface::Ptr ptr, std::size_t group_id);
		bool RegisterFilter(SingletonFilterInterface::Ptr ptr, std::size_t group_id);
		std::size_t ReleaseFilter(std::size_t group_id);

		struct ComponentArray
		{
			Archetype::OPtr ac;
			Archetype::ArrayMountPoint mp;
			std::span<std::size_t> output_archetype_locate;
		};

		struct EntityLocate
		{
			ComponentArray component_array;
			std::size_t mp_index;
		};

		ComponentArray ReadComponents(ComponentFilterInterface const& interface, std::size_t filter_ite, std::span<std::size_t> output_span) const;
		EntityLocate ReadEntity(Entity const& entity, ComponentFilterInterface const& interface, std::span<std::size_t> output_index) const;

		template<typename SingType, typename ...OT>
		Potato::Pointer::ObserverPtr<SingType> CreateSingletonType(OT&& ...ot)
		{
			using Type = SingletonType<std::remove_cvref_t<SingType>>;

			auto ID = UniqueTypeID::Create<typename Type::PureType>();

			std::lock_guard lg(singletons_mutex);
			auto f = std::find_if(singletons.begin(), singletons.end(), [=](SingletonElement const& ele)
			{
				return ele.id == ID;
			});
			if(f == singletons.end())
			{
				auto re = Potato::IR::MemoryResourceRecord::Allocate<Type>(&singleton_resource);
				if(re)
				{
					Type* ptr = new (re.Get()) Type {re, std::forward<OT>(ot)...};
					singletons.emplace_back(ptr, ID);
					std::lock_guard lg(filter_mapping_mutex);
					for(auto& ite : singleton_filters)
					{
						if(ite.rquire_id == ID)
						{
							ite.ptr->singleton_reference = SingletonInterface::Ptr{ ptr };
						}
					}
					return &ptr->Data;
				}
			}
			return nullptr;
		}

		bool ReleaseEntity(Entity::Ptr entity);

		Potato::Pointer::ObserverPtr<void> ReadSingleton(SingletonFilterInterface const& filter) const;

	protected:

		std::tuple<Archetype::Ptr, Archetype::ArrayMountPoint, std::size_t, std::size_t> CreateArchetype(std::span<ArchetypeID const> ids, std::span<std::size_t> output);
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
			ComponentPage::Ptr memory_page;
			std::size_t total_count;
		};

		mutable std::mutex component_mutex;
		std::pmr::vector<Element> components;
		std::pmr::unsynchronized_pool_resource components_resource;
		std::pmr::unsynchronized_pool_resource archetype_resource;

		struct SingletonElement
		{
			SingletonInterface::Ptr single;
			UniqueTypeID id;
		};

		mutable std::mutex singletons_mutex;
		std::pmr::vector<SingletonElement> singletons;
		std::optional<std::size_t> update_index;

		Archetype::ArrayMountPoint AllocateAndConstructMountPoint(Element& tar, Archetype::ArrayMountPoint mp, std::size_t mp_target);
		void CopyMountPointFormLast(Element& tar, Archetype::ArrayMountPoint mp, std::size_t mp_target);

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

		std::mutex spawn_mutex;
		std::pmr::vector<SpawnedElement> spawned_entities;
		bool need_update = false;
		

		struct CompFilterElement
		{
			ComponentFilterInterface::Ptr filter;
			std::size_t group_id;
		};

		struct SingletonFilterElement
		{
			SingletonFilterInterface::Ptr ptr;
			UniqueTypeID rquire_id;
			std::size_t group_id;
		};

		std::shared_mutex filter_mapping_mutex;
		std::pmr::vector<CompFilterElement> filter_mapping;
		std::pmr::vector<SingletonFilterElement> singleton_filters;

		std::pmr::synchronized_pool_resource singleton_resource;

		std::pmr::synchronized_pool_resource temp_resource;
	};

}