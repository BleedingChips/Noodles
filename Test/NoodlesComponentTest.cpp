import std;

#include <cassert>;

import NoodlesComponent;
import NoodlesGlobalContext;
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

	auto global_context = GlobalContext::Create();
	ComponentManager manager{global_context };

	std::pmr::vector<BitFlagConstContainer::Element> elements;
	elements.resize(global_context->GetComponentBitFlagContainerElementCount());

	auto init_list = std::array{
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<A>::Create(), *global_context->GetComponentBitFlag(*StructLayout::GetStatic<A>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<B>::Create(), *global_context->GetComponentBitFlag(*StructLayout::GetStatic<B>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<C>::Create(), *global_context->GetComponentBitFlag(*StructLayout::GetStatic<C>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<D>::Create(), *global_context->GetComponentBitFlag(*StructLayout::GetStatic<D>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<E>::Create(), *global_context->GetComponentBitFlag(*StructLayout::GetStatic<E>())},
	};

	for (auto& ite : init_list)
	{
		auto re = BitFlagContainer{elements}.SetValue(ite.flag);
		assert(re);
	}

	auto find = manager.LocateComponentChunk(BitFlagContainer{ elements });

	if (!find)
	{
		find = manager.CreateComponentChunk(init_list);
	}

	auto find2 = manager.LocateComponentChunk(BitFlagContainer{ elements });

	return 0;
}