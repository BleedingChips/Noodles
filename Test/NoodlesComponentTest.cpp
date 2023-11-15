import std;

import NoodlesComponent;
import NoodlesSystem;

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

	{
		std::vector<Noodles::ArchetypeID> ids{
		ArchetypeID::Create<A>(),
		ArchetypeID::Create<B>(),
		ArchetypeID::Create<C>(),
		ArchetypeID::Create<Report>(),
		};

		std::vector<Noodles::ArchetypeID> idscc{
			ArchetypeID::Create<A>(),
			ArchetypeID::Create<B>(),
			ArchetypeID::Create<C>(),
			ArchetypeID::Create<D>(),
			ArchetypeID::Create<Report>(),
		};

		ArchetypeComponentManager manager;

		auto entity = manager.CreateEntityDefer(std::span(ids), [](EntityConstructor& cons)
			{
				cons.Construct<A>(
					A{ 10 }
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

		//auto f1 = SystemComponentFilter::Create(std::span(ids2));

		//manager.RegisterComponentFilter(f1.GetPointer(), 0);

		manager.UpdateEntityStatus();

		//auto f2 = SystemComponentFilter::Create(std::span(ids2));

		//manager.RegisterComponentFilter(f2.GetPointer(), 0);

		std::size_t C = manager.ArchetypeCount();

		for (std::size_t i = 0; i < C; ++i)
		{
			manager.ForeachMountPoint(i, [](ArchetypeMountPointRange range)-> bool
				{
					auto I = range.archetype.LocateTypeID(UniqueTypeID::Create<A>());
					if (I.has_value())
					{
						for (auto ite = range.mp_begin; ite != range.mp_end; ++ite)
						{
							A* a = static_cast<A*>(range.archetype.GetData(I->index, 0, ite));
							volatile int i = 0;
						}
					}
					return true;
				});
		}

		manager.ReadEntityMountPoint(*entity, [](EntityStatus status, Archetype const& arc, ArchetypeMountPoint mp)
		{
				auto I = arc.LocateTypeID(UniqueTypeID::Create<A>());
				if (I.has_value())
				{
					A* a = static_cast<A*>(arc.GetData(I->index, 0, mp));
					volatile int i = 0;
				}
		});
	}

	return 0;
}