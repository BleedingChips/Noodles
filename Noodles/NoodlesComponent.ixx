module;

export module NoodlesComponent;

import std;
import PotatoMisc;
import PotatoPointer;
import PotatoIR;

import NoodlesMemory;
import NoodlesArchetype;

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

	struct ArchetypeComponentManager
	{

		

	public:

		struct Element
		{
			Archetype::Ptr archetype;
			ComponentPage::SPtr top_page;
			ComponentPage::SPtr last_page;
		};

		std::pmr::vector<Element> components;

	};


	struct ComponentMemoryPage
	{
		
		Potato::IR::Layout const AllocateLayout;
		std::size_t AcceptableCount = 0;

		std::span<std::byte> Datas;

		std::shared_mutex PageMutex;
		ComponentMemoryPage* LastPage = nullptr;
		ComponentMemoryPage* NextPage = nullptr;
		std::size_t AvailableCount = 0;
		std::size_t AppendCount = 0;
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