import std;

import NoodlesComponent;

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

	auto manager = StructLayoutManager::Create();
	ComponentManager comp_manager{manager};


	auto init_list = std::array{
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<A>::Create(), *manager->LocateComponent(*StructLayout::GetStatic<A>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<B>::Create(), *manager->LocateComponent(*StructLayout::GetStatic<B>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<C>::Create(), *manager->LocateComponent(*StructLayout::GetStatic<C>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<D>::Create(), *manager->LocateComponent(*StructLayout::GetStatic<D>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<E>::Create(), *manager->LocateComponent(*StructLayout::GetStatic<E>())},
	};

	auto init_list2 = std::array<StructLayoutWriteProperty, 2>
	{
		StructLayoutWriteProperty::Get<A>(),
		StructLayoutWriteProperty::Get<A const>()
	};

	auto refuse_component = std::array<StructLayout::Ptr, 1>
	{
		StructLayout::GetStatic<E>()
	};


	{



		ComponentManager comp_manager{*manager};

		{

			auto filter = ComponentFilter::Create(*manager, init_list2,
				refuse_component);

			ComponentManager::ArchetypeBuilderRef buildref{ comp_manager };

			buildref.Insert(StructLayout::GetStatic<A>(), *manager->LocateComponent(*StructLayout::GetStatic<A>()));
			buildref.Insert(StructLayout::GetStatic<B>(), *manager->LocateComponent(*StructLayout::GetStatic<B>()));
			buildref.Insert(StructLayout::GetStatic<C>(), *manager->LocateComponent(*StructLayout::GetStatic<C>()));
			buildref.Insert(StructLayout::GetStatic<D>(), *manager->LocateComponent(*StructLayout::GetStatic<D>()));

			auto [aptr, index] = comp_manager.FindOrCreateArchetype(buildref);

			auto [aptr2, index2] = comp_manager.FindOrCreateArchetype(buildref);

			comp_manager.UpdateFilter_AssumedLocked(*filter);

			std::array<void*, 100> buffer;
			ComponentAccessor accessor{buffer};
			auto re = comp_manager.ReadComponentRow_AssumedLocked(*filter, 0, accessor);

			volatile int i = 0;
		}

		

	}


	volatile int i = 0;

	return 0;
}