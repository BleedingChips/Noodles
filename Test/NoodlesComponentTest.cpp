import std;

import NoodlesComponent;
import NoodlesSystem;
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

template<typename ...AT>
struct TestComponentFilter : public ComponentFilterInterface
{
	virtual void AddFilterRef() const override {};
	virtual void SubFilterRef() const override {};
	virtual std::span<UniqueTypeID const> GetArchetypeIndex() const override
	{
		static std::array<UniqueTypeID, sizeof...(AT)> TemBuffer{UniqueTypeID::Create<std::remove_cvref_t<AT>>()...};
		return std::span(TemBuffer);
	}
	static constexpr std::size_t Count() { return sizeof...(AT); }
};

template<typename AT>
struct TestSingletonFilter : public SingletonFilterInterface
{
	virtual void AddFilterRef() const override {};
	virtual void SubFilterRef() const override {};
	virtual UniqueTypeID RequireTypeID() const override { return UniqueTypeID::Create<AT>(); }
};


int main()
{
	{
		TestComponentFilter<Report, A, EntityProperty> TF;
		TestSingletonFilter<A> ATF;

		ArchetypeComponentManager manager;


		auto entity = manager.CreateEntityDefer(
			Report{}, A{ 10086 }
		);

		manager.RegisterComponentFilter(&TF, 0);

		std::array<std::size_t, decltype(TF)::Count()> TemBuffer;

		manager.ReadyEntity(*entity, TF, TemBuffer.size(), std::span(TemBuffer),
			[](Archetype const& archetype, ArchetypeMountPoint mp, std::span<std::size_t> indexs)
			{
				auto D1 = static_cast<Report*>(archetype.GetData(indexs[0], mp));
				auto D2 = static_cast<A*>(archetype.GetData(indexs[1], mp));
				auto D3 = static_cast<EntityProperty*>(archetype.GetData(indexs[2], mp));
				volatile int i = 0;
			});


		auto K = manager.CreateSingletonType<A>(100);

		manager.RegisterSingletonFilter(&ATF, 0);


		auto entity2 = manager.CreateEntityDefer(
			Report{}, A{ 10086 }
		);

		auto entity3 = manager.CreateEntityDefer(
			Report{}, A{ 10086 }
		);

		manager.ReleaseEntity(entity2);

		manager.ForceUpdateState();

		volatile int i = 0;
	}
	

	
	
	/*
	Potato::IR::Layout layout{4, 4};
	Potato::IR::Layout layout2{ 4, 8 };
	bool io = layout > layout2;
	*/

	volatile int i = 0;





	/*
	{

		ArchetypeComponentManager manager;

		EntityConstructor e_const1;

		e_const1.MoveConstruct(Report{});
		e_const1.MoveConstruct(B{});
		e_const1.MoveConstruct(C{});
		e_const1.MoveConstruct(A{ 10 });
		e_const1.MoveConstruct(A{ 11 });
		e_const1.MoveConstruct(D{});

		auto entity = manager.CreateEntityDefer(e_const1);


		EntityConstructor e_const2;

		e_const2.MoveConstruct(Report{});
		e_const2.MoveConstruct(B{});
		e_const2.MoveConstruct(C{});
		e_const2.MoveConstruct(A{ 9});

		auto entity2 = manager.CreateEntityDefer(e_const2);

		EntityConstructor e_const3;

		e_const2.MoveConstruct(Report{});
		e_const2.MoveConstruct(B{});
		e_const2.MoveConstruct(C{});
		e_const2.MoveConstruct(A{ 8 });

		auto entity3 = manager.CreateEntityDefer(e_const3);

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
	*/

	return 0;
}