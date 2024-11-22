module;

#include <cassert>

export module NoodlesSingleton;

import std;
import PotatoTMP;
import PotatoMisc;
import PotatoPointer;
import PotatoIR;
import PotatoTaskSystem;

export import NoodlesArchetype;
import NoodlesComponent;
import NoodlesEntity;

export namespace Noodles
{

	export struct SingletonManager;
	struct SingletonView
	{
		Archetype::OPtr archetype;
		std::byte* buffer;
		std::byte* GetSingleton(
			Archetype::MemberView const& view
		) const;
		std::byte* GetSingleton(
			Archetype::Index index
		) const;
		std::byte* GetSingleton(
			MarkIndex index
		) const;
	};

	struct SingletonWrapper
	{
		std::pmr::vector<std::byte*> buffers;
		template<typename Type>
		Type* As(std::size_t index) const
		{
			auto b = reinterpret_cast<Type*>(buffers[index]);
			return (b == nullptr) ? nullptr : reinterpret_cast<Type*>(b);
		}
	};

	struct SingletonFilter : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Info = ComponentFilter::Info;
		using Ptr = Potato::Pointer::IntrusivePtr<SingletonFilter>;

		std::span<MarkElement const> GetRequiredAtomicMarkArray() const { return require_singleton; }
		std::span<MarkElement const> GetRequiresWriteAtomicMarkArray() const { return require_write_singleton; }
		std::span<MarkIndex const> GetMarkIndex() const { return mark_index; }

		bool OnSingletonModify(Archetype const& archetype);
		void Reset();

		static SingletonFilter::Ptr Create(
			StructLayoutMarkIndexManager& manager,
			std::span<Info const> require_singleton,
			std::pmr::memory_resource* storage_resource = std::pmr::get_default_resource()
		);

		std::span<std::size_t const> EnumSingleton_AssumedLocked() const { return archetype_offset; }

	protected:

		SingletonFilter(
			Potato::IR::MemoryResourceRecord record,
			std::span<MarkElement> require_singleton,
			std::span<MarkElement> require_write_singleton,
			std::span<MarkIndex> mark_index,
			std::span<std::size_t> archetype_offset
		)
			:MemoryResourceRecordIntrusiveInterface(record), require_singleton(require_singleton),
			require_write_singleton(require_write_singleton),
			mark_index(mark_index),
			archetype_offset(archetype_offset)
		{
		}

		std::span<MarkElement> require_singleton;
		std::span<MarkElement> require_write_singleton;
		std::span<MarkIndex> mark_index;

		mutable std::shared_mutex mutex;
		std::span<std::size_t> archetype_offset;

		friend struct SingletonManager;
		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
	};

	export struct SingletonManager
	{

		struct Config
		{
			std::size_t singleton_max_atomic_count = 128;
			std::pmr::memory_resource* singleton_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* resource = std::pmr::get_default_resource();
		};

		SingletonManager(Config config = {});
		~SingletonManager();

		SingletonFilter::Ptr CreateSingletonFilter(std::span<SingletonFilter::Info const> input, std::size_t identity, std::pmr::memory_resource* filter_resource = std::pmr::get_default_resource());

		SingletonWrapper ReadSingleton_AssumedLocked(SingletonFilter const& filter, std::pmr::memory_resource* wrapper_resource = std::pmr::get_default_resource()) const;
		bool AddSingleton(StructLayout::Ptr struct_layout, void* target_buffer, EntityManager::Operation operation, std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());
		template<typename SingletonType>
		bool AddSingleton(SingletonType&& type, std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource())
		{
			return this->AddSingleton(StructLayout::GetStatic<SingletonType>(), &type, std::is_rvalue_reference_v<SingletonType&&> ? EntityManager::Operation::Move : EntityManager::Operation::Copy, temp_resource);
		}

		bool RemoveSingleton(StructLayout::Ptr const& atomic_type);
		bool Flush(std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());
		SingletonView GetSingletonView_AssumedLocked() const
		{
			return { singleton_archetype, singleton_record.GetByte() };
		}

	protected:

		bool ClearCurrentSingleton_AssumedLocked();

		StructLayoutMarkIndexManager manager;

		std::shared_mutex mutex;
		Archetype::Ptr singleton_archetype;
		Potato::IR::MemoryResourceRecord singleton_record;
		std::pmr::unsynchronized_pool_resource singleton_resource;

		struct FilterTuple
		{
			SingletonFilter::Ptr filter;
			OptionalIndex identity;
		};

		std::shared_mutex filter_mutex;
		std::pmr::vector<FilterTuple> filter;

		struct Modify
		{
			MarkIndex mark_index;
			Potato::IR::MemoryResourceRecord resource;
			StructLayout::Ptr struct_layout;
			bool Release();
		};

		std::shared_mutex modifier_mutex;
		std::pmr::vector<MarkElement> modify_mask;
		std::pmr::vector<Modify> modifier;
		
	};


	/*
	template<bool mutex>
	struct NoodlesTypeProperty
	{
		static constexpr bool ignore_mutex = !mutex;
	};

	using IgnoreMutexProperty = NoodlesTypeProperty<false>;

	struct OptionalIndex
	{
		std::size_t real_index = std::numeric_limits<std::size_t>::max();
		operator bool() const { return real_index != std::numeric_limits<std::size_t>::max(); }
		operator std::size_t() const { assert(*this); return real_index; }
		OptionalIndex& operator=(OptionalIndex const&) = default;
		OptionalIndex& operator=(std::size_t input) { assert(input != std::numeric_limits<std::size_t>::max()); real_index = input; return *this; }
		OptionalIndex() {};
		OptionalIndex(OptionalIndex const&) = default;
		OptionalIndex(std::size_t input) : real_index(input) {}
		void Reset() { real_index = std::numeric_limits<std::size_t>::max(); }
		std::size_t Get() const { return real_index; }
	};


	export struct ArchetypeComponentManager;
	export struct EntityProperty;

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
		PendingDestroy,
	};

	struct Entity : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{

		using Ptr = Potato::Pointer::IntrusivePtr<Entity>;
		
		static Ptr Create(AtomicTypeManager const& component_manager, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

	protected:

		void SetFree();

		Entity(
			Potato::IR::MemoryResourceRecord record,
			std::span<AtomicTypeMark> current_component_mask,
			std::span<AtomicTypeMark> modify_component_mask
			) : MemoryResourceRecordIntrusiveInterface(record),
			current_component_mask(current_component_mask),
			modify_component_mask(modify_component_mask)
		{}

		~Entity();
		

		Potato::IR::MemoryResourceRecord record;
		mutable std::shared_mutex mutex;

		EntityStatus status = EntityStatus::PreInit;

		OptionalIndex archetype_index;
		OptionalIndex mount_point_index;
		OptionalIndex modify_index;

		std::span<AtomicTypeMark> current_component_mask;
		std::span<AtomicTypeMark> modify_component_mask;


		friend struct ArchetypeComponentManager;
		friend struct EntityProperty;

		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
	};

	using EntityPtr = Entity::Ptr;


	export struct EntityProperty
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

	struct ComponentFilter : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<ComponentFilter>;


		std::span<AtomicTypeMark const> GetRequiredAtomicMarkArray() const { return require_component; }
		std::span<AtomicTypeMark const> GetRequiresWriteAtomicMarkArray() const { return require_write_component; }
		std::span<AtomicTypeMark const> GetRefuseAtomicMarkArray() const { return refuse_component; }
		std::span<AtomicTypeID const> GetAtomicID() const { return atomic_id; }

		struct Info
		{
			bool need_write = false;
			AtomicType::Ptr atomic_type;
		};

		static ComponentFilter::Ptr Create(
			AtomicTypeManager& manager,
			std::span<Info const> require_component_type,
			std::span<AtomicType::Ptr const> refuse_component_type,
			std::pmr::memory_resource* storage_resource = std::pmr::get_default_resource(),
			std::pmr::memory_resource* archetype_info_resource = std::pmr::get_default_resource()
		);

		bool OnCreatedArchetype(std::size_t archetype_index, Archetype const& archetype);
		std::optional<std::span<std::size_t const>> EnumMountPointIndexByArchetypeIndex_AssumedLocked(std::size_t archetype_index) const;
		std::optional<std::span<std::size_t const>> EnumMountPointIndexByIterator_AssumedLocked(std::size_t iterator, std::size_t& archetype_index) const;

	protected:

		ComponentFilter(
			Potato::IR::MemoryResourceRecord record, 
			std::span<AtomicTypeMark> require_component,
			std::span<AtomicTypeMark> require_write_component,
			std::span<AtomicTypeMark> refuse_component,
			std::span<AtomicTypeID> atomic_id,
			std::pmr::memory_resource* resource
		)
			:MemoryResourceRecordIntrusiveInterface(record), require_component(require_component),
			require_write_component(require_write_component),
			refuse_component(refuse_component),
			atomic_id(atomic_id),
			archetype_offset(resource)
		{
			
		}

		std::span<AtomicTypeMark> require_component;
		std::span<AtomicTypeMark> require_write_component;
		std::span<AtomicTypeMark> refuse_component;
		std::span<AtomicTypeID> atomic_id;

		mutable std::shared_mutex mutex;
		std::pmr::vector<std::size_t> archetype_offset;

		friend struct ArchetypeComponentManager;
		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
	};

	

	export struct ArchetypeComponentManager
	{

		struct Setting
		{
			std::size_t max_component_atomic_type_count = 128;
			std::size_t max_singleton_atomic_type_count = 128;
		};

		ArchetypeComponentManager(Setting setting, std::pmr::memory_resource* resource = std::pmr::get_default_resource());
		~ArchetypeComponentManager();

		EntityPtr CreateEntity(std::pmr::memory_resource* entity_resource = std::pmr::get_default_resource(), std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());

		template<typename Type>
		bool AddEntityComponent(Entity& target_entity, Type&& type) requires(std::is_rvalue_reference_v<decltype(type)>) { return AddEntityComponent(target_entity, *GetAtomicType<Type>(), &type); }

		bool AddEntityComponent(Entity& target_entity, AtomicType const& archetype_id, void* reference_buffer);

		template<typename Type>
		bool RemoveEntityComponent(Entity& target_entity) { return RemoveEntityComponent(target_entity,  *GetAtomicType<Type>()); }

		bool RemoveEntityComponent(Entity& target_entity, AtomicType const& atomic_type, bool accept_build_in_component = false, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		bool ForceUpdateState(
			std::pmr::memory_resource* component_resource = std::pmr::get_default_resource(),
			std::pmr::memory_resource* archetype_resource = std::pmr::get_default_resource(),
			std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource()
			);

		ComponentFilter::Ptr CreateComponentFilter(
			std::span<ComponentFilter::Info const> require_component, 
			std::span<AtomicType::Ptr const> refuse_component, 
			std::size_t identity, 
			std::pmr::memory_resource* filter_resource = std::pmr::get_default_resource(),
			std::pmr::memory_resource* offset_resource = std::pmr::get_default_resource()
			);

		SingletonFilter::Ptr CreateSingletonFilter(
			std::span<SingletonFilter::Info const> require_singleton, 
			std::size_t identity, 
			std::pmr::memory_resource* filter_resource = std::pmr::get_default_resource()
			);

		struct DataWrapper
		{
			Archetype::OPtr archetype;
			std::size_t data_array_count;
			std::pmr::vector<void*> elements;
			operator bool() const { return archetype; }
		};

		std::optional<DataWrapper> ReadComponents_AssumedLocked(ComponentFilter const& filter, std::size_t filter_ite, std::pmr::memory_resource* wrapper_resource = std::pmr::get_default_resource()) const;
		std::optional<DataWrapper> ReadEntityComponents_AssumedLocked(Entity const& ent, ComponentFilter const& filter, std::pmr::memory_resource* wrapper_resource = std::pmr::get_default_resource()) const;
		//std::optional<std::span<void*>> ReadEntityDirect_AssumedLocked(Entity const& entity, ComponentFilter const& filter, std::span<void*> output_ptr, bool prefer_modify = true) const;

		DataWrapper ReadSingleton_AssumedLocked(SingletonFilter const& filter, std::pmr::memory_resource* wrapper_resource = std::pmr::get_default_resource()) const;

		bool MoveAndCreateSingleton(AtomicType::Ptr atomic_type, void* move_reference, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		template<typename Type>
		bool MoveAndCreateSingleton(Type && reference, std::pmr::memory_resource* resource = std::pmr::get_default_resource()) { return this->MoveAndCreateSingleton(GetAtomicType<Type>(), &reference, resource); }

		bool ReleaseEntity(Entity& entity, std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());

	protected:

		bool AddEntityComponent_AssumedLocked(Entity& target_entity, AtomicType const& archetype_id, void* reference_buffer, bool accept_build_in_component, std::pmr::memory_resource* resource);

		AtomicTypeID GetEntityPropertyAtomicTypeID() const { return AtomicTypeID{0}; }

		std::tuple<Archetype::OPtr, Archetype::ArrayMountPoint> GetComponentPage(std::size_t archetype_index) const;


		static bool CheckIsSameArchetype(Archetype const& target, std::size_t hash_code, std::span<AtomicType::Ptr const> ids);
		

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
			Archetype::Index entity_property_locate;
			std::size_t total_count;
		};

		AtomicTypeManager component_manager;
		AtomicTypeManager singleton_manager;

		mutable std::shared_mutex component_mutex;
		std::pmr::vector<Element> components;

		Archetype::Ptr singleton_archetype;
		Potato::IR::MemoryResourceRecord singleton_record;

		std::optional<std::size_t> AllocateMountPoint(Element& tar, std::pmr::memory_resource* component_resource);
		void CopyMountPointFormLast(Element& tar, std::size_t mp_index);

		std::mutex entity_modifier_mutex;

		struct EntityModifierEvent
		{

			enum class Operation
			{
				Ignore,
				AddComponent,
				RemoveComponent,
			};

			Operation operation = Operation::Ignore;
			AtomicTypeID index;
			AtomicType::Ptr atomic_type;
			Potato::IR::MemoryResourceRecord resource;
			~EntityModifierEvent();
			EntityModifierEvent(Operation operation, AtomicTypeID index, AtomicType::Ptr atomic_type, Potato::IR::MemoryResourceRecord resource)
				: operation(operation), index(index), atomic_type(std::move(atomic_type)), resource(std::move(resource)) {}
			EntityModifierEvent(EntityModifierEvent&& event);
		};

		struct EntityModifier
		{
			Entity::Ptr reference_entity;
			bool need_remove = false;
			std::pmr::vector<EntityModifierEvent> modifier_event;
		};

		std::pmr::vector<EntityModifier> modified_entity;

		struct SingletonModifier
		{
			AtomicTypeID index;
			AtomicType::Ptr atomic_type;
			Potato::IR::MemoryResourceRecord resource;
			~SingletonModifier();
		};

		std::mutex singleton_modifier_mutex;
		std::pmr::vector<SingletonModifier> singleton_modifier;
		std::pmr::vector<AtomicTypeMark> singleton_modifier_mark;

		std::shared_mutex filter_mutex;
		std::pmr::vector<std::tuple<ComponentFilter::Ptr, OptionalIndex>> component_filter;
		bool component_filter_need_remove = false;
		std::pmr::vector< std::tuple<SingletonFilter::Ptr, OptionalIndex>> singleton_filters;
		bool singleton_filter_need_remove = false;
	};
	*/
}