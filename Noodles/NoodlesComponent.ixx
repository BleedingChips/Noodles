module;

export module NoodlesComponent;

import std;
import PotatoMisc;
import PotatoPointer;
import PotatoIR;

import NoodlesMemory;
import NoodlesArchetype;
import NoodlesEntity;

export namespace Noodles
{

	struct ComponentPage : public Potato::Pointer::DefaultStrongWeakInterface
	{
		using SPtr = Potato::Pointer::StrongPtr<ComponentPage>;
		using WPtr = Potato::Pointer::WeakPtr<ComponentPage>;

		static ComponentPage::SPtr Create(
			Potato::IR::Layout component_layout, std::size_t min_element_count, std::size_t min_page_size, std::pmr::memory_resource* resouce
		);

		WPtr last_page;
		SPtr next_page;

		std::size_t max_element_count;
		std::size_t available_count;
		std::span<std::size_t> buffer;
		std::size_t allocate_size;
		std::pmr::memory_resource* resource;
	};

	struct EntityProperty
	{
		Entity entity;
		std::pmr::vector<std::size_t> flags;
	};

	struct ArchetypeComponentManager
	{

		Archetype::Ptr CreateArchetype(std::span<ArchetypeID const> ids);

		Entity CreateEntity(Archetype::Ptr ptr);


	public:

		struct Element
		{
			Archetype::Ptr archetype;
			ComponentPage::SPtr top_page;
			ComponentPage::SPtr last_page;
		};

		std::pmr::vector<Element> components;

		struct SpawnedComponent
		{
			std::optional<std::size_t> exist_archetype;
			Archetype::Ptr archetype;
			void* data;
		};

		std::mutex spawn_mutex;
		std::pmr::vector<SpawnedComponent> spawned_entities;

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