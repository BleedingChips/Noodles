import std;
import NoodlesComponent;

using namespace Noodles;

struct alignas(128) A{ std::size_t i = 0; };

struct alignas(64) B { std::size_t i = 1; };

struct alignas(32) C { std::size_t i = 1; };

struct alignas(32) D { std::size_t i = 1; };

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

	std::vector<Noodles::ArchetypeID> ids{
		ArchetypeID::Create<A>(),
		ArchetypeID::Create<B>(),
		ArchetypeID::Create<C>()
	};

	std::vector<Noodles::ArchetypeID> idscc{
		ArchetypeID::Create<A>(),
		ArchetypeID::Create<B>(),
		ArchetypeID::Create<C>(),
		ArchetypeID::Create<D>(),
	};

	ArchetypeComponentManager Manager;

	auto entity = Manager.CreateEntityDefer(std::span(ids), [](EntityConstructor& cons)
	{
	});

	auto entity2 = Manager.CreateEntityDefer(std::span(ids), [](EntityConstructor& cons)
	{
	});

	auto entity3 = Manager.CreateEntityDefer(std::span(idscc), [](EntityConstructor& cons)
		{
		});

	//Manager.DestroyEntity(entity);

	std::vector<Noodles::UniqueTypeID> ids2{
		UniqueTypeID::Create<A>(),
		UniqueTypeID::Create<B>(),
		UniqueTypeID::Create<D>()
	};

	auto a_size = ComponentFilterWrapper::UniqueAndSort(std::span(ids2));
	ids2.erase(ids2.begin() + a_size, ids2.end());

	//std::sort(ids2.begin(), ids2.end());

	auto f1 = Manager.CreateFilter(std::span(ids2));

	Manager.UpdateEntityStatus();

	auto f2 = Manager.CreateFilter(std::span(ids2));

	std::vector<std::size_t> ids3 = {
		*f1->LocateTypeIDIndex(UniqueTypeID::Create<A>()),
		*f1->LocateTypeIDIndex(UniqueTypeID::Create<B>()),
		*f1->LocateTypeIDIndex(UniqueTypeID::Create<C>()),
	};

	for(auto ite : *f2)
	{
		volatile int i = 0;
	}

	return 0;
}