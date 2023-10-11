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

}