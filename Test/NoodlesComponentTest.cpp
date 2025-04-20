import std;

#include <cassert>

import NoodlesComponent;
import NoodlesClassBitFlag;
import NoodlesBitFlag;
import NoodlesArchetype;

import PotatoIR;

using namespace Noodles;

struct alignas(128) A{ std::size_t i = 0; };

struct alignas(64) B { std::size_t i = 1; };

struct alignas(32) C { std::size_t i = 1; };

struct alignas(32) D { std::size_t i = 1; };

static std::int32_t index = 0;

struct alignas(8) Report{
	Report() { index += 1; }
	Report(Report const&) { index += 1; }
	Report(Report &&) { index += 1; }
	~Report() { index -= 1; }
};

struct Tot
{
	A a;
	B b;
	C c;
};

struct E
{
	std::size_t index = 1;
	using NoodlesSingletonRequire = std::size_t;
};


int main()
{

	AsynClassBitFlagMap map;

	ComponentManager manager{};

	BitFlagContainer bitflag{ map.GetBitFlagContainerElementCount() };

	auto init_list = std::array{
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<A>::Create(), *map.LocateOrAdd(*StructLayout::GetStatic<A>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<B>::Create(), *map.LocateOrAdd(*StructLayout::GetStatic<B>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<C>::Create(), *map.LocateOrAdd(*StructLayout::GetStatic<C>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<D>::Create(), *map.LocateOrAdd(*StructLayout::GetStatic<D>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<E>::Create(), *map.LocateOrAdd(*StructLayout::GetStatic<E>())},
	};

	for (auto& ite : init_list)
	{
		auto re = BitFlagContainerViewer{ bitflag }.SetValue(ite.flag);
		assert(re);
	}

	auto find = manager.LocateComponentChunk(BitFlagContainerViewer{ bitflag });

	if (!find)
	{
		find = manager.CreateComponentChunk(init_list);
	}

	auto find2 = manager.LocateComponentChunk(bitflag);

	return 0;
}