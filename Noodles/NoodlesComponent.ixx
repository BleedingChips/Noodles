module;

export module NoodlesComponent;

import std;
import PotatoMisc;
import PotatoPointer;
import PotatoIR;

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

		ComponentPage(
			std::size_t max_element_count, 
			std::size_t allocate_size,
			std::pmr::memory_resource* upstream,
			std::span<std::byte> buffer
			);

		Ptr next_page;

		ArchetypeMountPoint GetLastPointPoint() const
		{
			return {
				buffer.data(),
				max_element_count,
				available_count
			};
		}

	protected:

		std::size_t const max_element_count = 0;
		std::size_t available_count = 0;
		std::span<std::byte> const buffer;
		std::size_t const allocate_size = 0;
		std::pmr::memory_resource* const resource = nullptr;
	};

	struct EntityProperty
	{
		Entity entity;
		std::pmr::vector<std::size_t> flags;
	};

	struct ArchetypeComponentManager
	{

		struct EntityConstructor
		{

			template<typename Type>
			bool Construct(Type&& type, std::size_t i = 0)
			{
				return Construct(UniqueTypeID::Create<std::remove_cvref_t<Type>>(), &type, i);
			}

			bool Construct(UniqueTypeID const& id, void* source, std::size_t i = 0);

			EntityConstructor(EntityConstructor&&) = default;
			EntityConstructor() = default;

		protected:

			EntityConstructor(
				Archetype::Ptr archetype_ptr,
				ArchetypeMountPoint mount_point
			): archetype_ptr(std::move(archetype_ptr)),
				mount_point(mount_point)
			{
				status.resize(this->archetype_ptr->GetTypeIDCount());
			}

			Archetype::Ptr archetype_ptr;
			ArchetypeMountPoint mount_point;
			std::vector<std::uint8_t> status;

			friend struct ArchetypeComponentManager;
		};

		EntityConstructor CreateEntityConstructor(std::span<ArchetypeID const> ids);
		Entity CreateEntity(EntityConstructor&& constructor);

		ArchetypeComponentManager(std::pmr::memory_resource* upstream = std::pmr::get_default_resource());

	protected:

		struct Element
		{
			Archetype::Ptr archetype;
			ComponentPage::Ptr top_page;
			ComponentPage::Ptr last_page;
		};

		std::pmr::vector<Element> components;

		std::pmr::memory_resource* resource;

		std::mutex spawn_mutex;
		std::pmr::vector<Entity> spawned_entities;

		std::mutex spawned_entities_resource_mutex;
		std::pmr::monotonic_buffer_resource spawned_entities_resource;

		std::mutex archetype_resource_mutex;
		std::pmr::monotonic_buffer_resource archetype_resource;

		Memory::IntrusiveMemoryResource<std::pmr::synchronized_pool_resource>::Ptr entity_resource;

		std::mutex remove_mutex;
		std::pmr::vector<Entity> removed_entities;

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