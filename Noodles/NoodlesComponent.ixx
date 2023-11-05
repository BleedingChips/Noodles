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

	struct EntityProperty
	{
		Entity entity;
		std::pmr::vector<std::size_t> flags;
	};

	

	struct EntityConstructor
	{

		enum class Status
		{
			Done,
			BadArchetype,
			BadMemoryResource
		};

		template<typename Type>
		bool Construct(Type&& type, std::size_t i = 0)
		{
			return Construct(UniqueTypeID::Create<std::remove_cvref_t<Type>>(), &type, i);
		}

		bool Construct(UniqueTypeID const& id, void* source, std::size_t i = 0);

		operator bool() const { return status == Status::Done; }

	protected:

		EntityConstructor(EntityConstructor&&) = default;

		EntityConstructor(Status status) : status(status) {}

		EntityConstructor(
			Status status,
			Archetype::Ptr archetype_ptr,
			ArchetypeMountPoint mount_point,
			std::pmr::memory_resource* resource
		) : status(status), archetype_ptr(std::move(archetype_ptr)),
			mount_point(mount_point), construct_record(resource)
		{
			assert(*this);
			construct_record.resize(this->archetype_ptr->GetTypeIDCount());
		}

		Status status = Status::Done;
		Archetype::Ptr archetype_ptr;
		ArchetypeMountPoint mount_point;
		std::pmr::vector<std::uint8_t> construct_record;

		friend struct ArchetypeComponentManager;
	};


	struct ComponentFilterWrapper : public Potato::Task::ControlDefaultInterface
	{

		using CPtr = Potato::Task::ControlPtr<ComponentFilterWrapper>;
		using Ptr = Potato::Pointer::IntrusivePtr<ComponentFilterWrapper>;

		static CPtr Create(std::span<UniqueTypeID const> ids, std::pmr::memory_resource* resource);

	protected:

		ComponentFilterWrapper(
			std::span<std::byte> buffer,
			std::span<UniqueTypeID const> ref_ids, 
			std::size_t allocated_size, std::pmr::memory_resource* resource
			);

		virtual ~ComponentFilterWrapper();

		virtual void Release() override;
		virtual void ControlRelease() override {}

		std::span<UniqueTypeID> capture_info;
		std::size_t allocated_size = 0;
		std::pmr::memory_resource* resource = nullptr;

		struct InDirectMapping
		{
			UniqueTypeID id;
			std::size_t element_index;
			Potato::Misc::IndexSpan<> pre_calculate_index;
		};

		std::pmr::vector<InDirectMapping> in_direct_mapping;
		std::pmr::vector<std::size_t> pre_calculate_index;
	};

	struct ArchetypeComponentManager
	{

		ArchetypeComponentManager(std::pmr::memory_resource* upstream = std::pmr::get_default_resource());
		~ArchetypeComponentManager();
		template<typename Func>
		Entity CreateEntityDefer(std::span<ArchetypeID const> ids, Func&& func, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
			requires(std::is_invocable_v<Func, EntityConstructor&>)
		{
			auto Constructor = PreCreateEntityImp(ids, resource);
			if(Constructor)
			{
				func(Constructor);
				return CreateEntityImp(Constructor);
			}
			return {};
		}

		bool UpdateEntityStatus();
		bool DestroyEntity(Entity entity);

		ComponentFilterWrapper::CPtr CreateFilter(std::span<UniqueTypeID const> ids, std::pmr::memory_resource* resource);

	protected:

		static void ReleaseEntity(EntityStorage& storage);

		EntityConstructor PreCreateEntityImp(std::span<ArchetypeID const> ids, std::pmr::memory_resource* resource);
		Entity CreateEntityImp(EntityConstructor& constructor);

		struct Element
		{
			Archetype::Ptr archetype;
			ComponentPage::Ptr top_page;
			ComponentPage::Ptr last_page;
		};

		std::shared_mutex components_mutex;
		std::pmr::vector<Element> components;
		std::pmr::synchronized_pool_resource components_resource;

		std::mutex spawn_mutex;
		std::pmr::vector<Entity> spawned_entities;
		std::pmr::monotonic_buffer_resource spawned_entities_resource;
		bool need_update = false;

		struct RemoveEntity
		{
			Entity entity;
			EntityStatus last_status;
		};

		std::pmr::vector<RemoveEntity> removed_entities;

		std::mutex archetype_resource_mutex;
		std::pmr::monotonic_buffer_resource archetype_resource;

		std::mutex filter_mapping_mutex;
		std::pmr::vector<ComponentFilterWrapper::Ptr> filter_mapping;

		Memory::IntrusiveMemoryResource<std::pmr::synchronized_pool_resource>::Ptr entity_resource;

	};

	template<typename ...Components>
	struct ComponentFilter
	{
		static_assert(sizeof...(Components) >= 1, "Component Filter Require At Least One Component");
	};

	template<typename GlobalComponent>
	struct GlobalComponentFilter
	{
		
	};

	export template<typename T>
	struct IsAcceptableComponentFilter
	{
		static constexpr bool Value = false;
	};

	export template<typename ...T>
	struct IsAcceptableComponentFilter<ComponentFilter<T...>>
	{
		static constexpr bool Value = true;
	};

	export template<typename T>
	constexpr bool IsAcceptableComponentFilterV = IsAcceptableComponentFilter<T>::Value;

	/*
	template<typename T>
	struct IsAcceptableGobalComponentFilter
	{
		static constexpr bool Value = false;
	};
	*/

}