module;

export module NoodlesComponent;

import std;
import PotatoMisc;
import PotatoPointer;
import PotatoIR;

import NoodlesMemory;

export namespace Noodles
{

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