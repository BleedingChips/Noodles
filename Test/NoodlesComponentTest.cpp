import std;
import NoodlesComponent;
import NoodlesSystem;

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

	ArchetypeComponentManager manager;

	auto entity = manager.CreateEntityDefer(std::span(ids), [](EntityConstructor& cons)
	{
			cons.Construct<A>(
				A{10}
			);
	});

	auto entity2 = manager.CreateEntityDefer(std::span(ids), [](EntityConstructor& cons)
	{
			cons.Construct<A>(
				A{ 9 }
			);
	});

	auto entity3 = manager.CreateEntityDefer(std::span(idscc), [](EntityConstructor& cons)
		{
			cons.Construct<A>(
				A{ 8 }
			);
		});

	//Manager.DestroyEntity(entity);

	std::vector<SystemRWInfo> ids2{
		SystemRWInfo::Create<A>(),
		SystemRWInfo::Create<B>()
	};

	auto f1 = SystemComponentFilter::Create(std::span(ids2));

	manager.RegisterComponentFilter(f1.GetPointer(), 0);

	manager.UpdateEntityStatus();

	auto f2 = SystemComponentFilter::Create(std::span(ids2));

	manager.RegisterComponentFilter(f2.GetPointer(), 0);

	for(auto ite : *f1)
	{
		auto range = manager.GetArchetypeMountPointRange(ite.element_index);

		for(auto ite2 : *range)
		{
			auto a1 = static_cast<A*>();
		}


		Manager.ForeachMountPoint(ite, [&](MountPointRange range)
		{
			for(auto& ite2 : range)
			{
				auto ref = ite.indexs[ids3[0]];
				auto Aress = static_cast<A*>(range.archetype.GetData(ref.index, ite2));
				volatile int i = 0;
			}
		});
	}
	Manager.ReadEntity(*entity2, [](Archetype const& arc, ArchetypeMountPoint mp)
	{
		auto Aress = static_cast<A*>(arc.GetData(*arc.LocateTypeID(UniqueTypeID::Create<A>()), mp));
		auto Aress2 = static_cast<B*>(arc.GetData(*arc.LocateTypeID(UniqueTypeID::Create<B>()), mp));
		auto Aress3 = static_cast<C*>(arc.GetData(*arc.LocateTypeID(UniqueTypeID::Create<C>()), mp));
		volatile int o = 0;
	});


	return 0;
}