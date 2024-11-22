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

	StructLayoutMarkIndexManager manager;


	auto init_list = std::array{
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<A>::Create(), *manager.LocateOrAdd(StructLayout::GetStatic<A>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<B>::Create(), *manager.LocateOrAdd(StructLayout::GetStatic<B>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<C>::Create(), *manager.LocateOrAdd(StructLayout::GetStatic<C>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<D>::Create(), *manager.LocateOrAdd(StructLayout::GetStatic<D>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<E>::Create(), *manager.LocateOrAdd(StructLayout::GetStatic<E>())},
	};

	auto init_list2 = std::array<ComponentFilter::Info, 2>
	{
		ComponentFilter::Info{true, Potato::IR::StaticAtomicStructLayout<A>::Create()},
		ComponentFilter::Info{false, Potato::IR::StaticAtomicStructLayout<B>::Create()}
	};

	auto refuse_component = std::array<StructLayout::Ptr, 1>
	{
		StructLayout::GetStatic<E>()
	};


	{
		

		

		auto archetype = Archetype::Create(manager, init_list);

		
		

		auto filter = ComponentFilter::Create(manager, init_list2, refuse_component);

		filter->OnCreatedArchetype(0, *archetype);

		volatile std::size_t k2 = 0;
	}


	{



		ComponentManager manager;

		{

			auto ptr = manager.CreateComponentFilter(
				init_list2,
				refuse_component,
				0
			);

			ComponentManager::ArchetypeBuilderRef buildref{manager};
			buildref.Insert(StructLayout::GetStatic<A>(), *manager.LocateStructLayout(StructLayout::GetStatic<A>()));
			buildref.Insert(StructLayout::GetStatic<B>(), *manager.LocateStructLayout(StructLayout::GetStatic<B>()));
			buildref.Insert(StructLayout::GetStatic<C>(), *manager.LocateStructLayout(StructLayout::GetStatic<C>()));
			buildref.Insert(StructLayout::GetStatic<D>(), *manager.LocateStructLayout(StructLayout::GetStatic<D>()));

			auto [aptr, index] = manager.FindOrCreateArchetype(buildref);

			auto [aptr2, index2] = manager.FindOrCreateArchetype(buildref);
		}

	}


	volatile int i = 0;

	return 0;
}