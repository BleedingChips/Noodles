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
		TestComponentFilter<Report, A, EntityProperty, std::u8string> TF;
		TestSingletonFilter<A> ATF;

		ArchetypeComponentManager manager;

		auto e1 = manager.CreateEntity();

		manager.AddEntityComponent(*e1, Report{});
		manager.AddEntityComponent(*e1, A{ 10086 });
		manager.AddEntityComponent(*e1, std::u8string{u8"Fuck You"});

		/*
		auto entity = manager.CreateEntityDefer(
			std::pmr::get_default_resource(), Report{}, A{ 10086 }, std::u8string{u8"Fuck You"}
		);
		*/

		manager.RegisterFilter(&TF, 0);

		std::array<std::size_t, decltype(TF)::Count()> TemBuffer;

		/*
		{
			auto p = manager.ReadEntity(*entity, TF, std::span(TemBuffer));

			if (p)
			{
				auto D1 = static_cast<Report*>(p.GetRawData(0)) ;
				auto D2 = static_cast<A*>(p.GetRawData(1));
				auto D3 = static_cast<EntityProperty*>(p.GetRawData(2));
				auto D4 = static_cast<std::u8string*>(p.GetRawData(3));

				auto i1 = p.GetRawData(0);
				auto i2 = p.GetRawData(1);
				auto i3 = p.GetRawData(2);
				auto i4 = p.GetRawData(3);
				*D4 = u8"Lalalal";
				volatile int i = 0;
			}
		}
		*/


		auto K = manager.CreateSingletonType<A>(100);

		manager.RegisterFilter(&ATF, 0);

		auto k = manager.ReadSingleton(ATF);
		if(k != nullptr)
		{
			auto op = reinterpret_cast<A*>(k.GetPointer());
			volatile int i = 0;
		}

		/*

		auto entity2 = manager.CreateEntityDefer(
			std::pmr::get_default_resource(),
			Report{}, A{ 10086 }
		);

		auto entity3 = manager.CreateEntityDefer(
			std::pmr::get_default_resource(),
			Report{}, A{ 10086 }
		);

		TestComponentFilter<Report, A, EntityProperty, std::u8string, C> TF2;

		manager.RegisterFilter(&TF2, 0);

		for(std::size_t i = 0; i < 1000; ++i)
		{
			manager.CreateEntityDefer(
				std::pmr::get_default_resource(),
				Report{}, A{i}, std::u8string{u8"FastTest"}, C{i * 2}
			);
		}

		std::array<std::size_t, TF2.Count()> TemBuffer2;
		*/
		manager.ForceUpdateState();

		auto e2 = manager.CreateEntity();

		manager.AddEntityComponent(*e2, Report{});
		manager.AddEntityComponent(*e2, A{ 10086 });
		manager.AddEntityComponent(*e2, std::u8string{u8"Fuck You"});

		manager.ReleaseEntity(*e2);

		manager.ForceUpdateState();

		/*
		{
			std::size_t total = 0;
			std::size_t ite = 0;
			while(true)
			{
				auto wra = manager.ReadComponents(TF2, ite++, std::span(TemBuffer2));
				

				if(wra)
				{
					auto p1 = wra.GetRawArray(1).Translate<A>();
					auto p2 = wra.GetRawArray(3).Translate<std::u8string>();
					auto p3 = wra.GetRawArray(4).Translate<std::u8string>();


					volatile int i = 0;
					
				}else
				{
					break;
				}
			}

			volatile int icc = 0;
		}
		*/
		/*
		{
			auto [ar, mpb, mpe, i] = manager.ReadComponents(TF, 0, std::span(TemBuffer));
			if (ar && mpb)
			{
				auto D1 = static_cast<Report*>(ar->GetData(TemBuffer[0], mpb));
				auto D2 = static_cast<A*>(ar->GetData(TemBuffer[1], mpb));
				auto D3 = static_cast<EntityProperty*>(ar->GetData(TemBuffer[2], mpb));
				auto D4 = static_cast<std::u8string*>(ar->GetData(TemBuffer[3], mpb));
				volatile int i = 0;
			}
		}
		

		

		{
			auto [ar, mp, in] = manager.ReadEntity(*entity, TF, std::span(TemBuffer));

			if (ar)
			{
				auto D1 = static_cast<Report*>(ar->GetData(TemBuffer[0], mp));
				auto D2 = static_cast<A*>(ar->GetData(TemBuffer[1], mp));
				auto D3 = static_cast<EntityProperty*>(ar->GetData(TemBuffer[2], mp));
				auto D4 = static_cast<std::u8string*>(ar->GetData(TemBuffer[3], mp));
				volatile int i = 0;
			}
		}

		{
			auto [ar, mpb, mpe, i] = manager.ReadComponents(TF, 0, std::span(TemBuffer));
			if (ar && mpb)
			{
				auto D1 = static_cast<Report*>(ar->GetData(TemBuffer[0], mpb));
				auto D2 = static_cast<A*>(ar->GetData(TemBuffer[1], mpb));
				auto D3 = static_cast<EntityProperty*>(ar->GetData(TemBuffer[2], mpb));
				auto D4 = static_cast<std::u8string*>(ar->GetData(TemBuffer[3], mpb));
				volatile int i = 0;
			}
		}

		volatile int i = 0;

		auto fc = manager.ReleaseFilter(0);

		{
			auto [ar, mpb, mpe, i] = manager.ReadComponents(TF, 0, std::span(TemBuffer));
			if (ar && mpb)
			{
				auto D1 = static_cast<Report*>(ar->GetData(TemBuffer[0], mpb));
				auto D2 = static_cast<A*>(ar->GetData(TemBuffer[1], mpb));
				auto D3 = static_cast<EntityProperty*>(ar->GetData(TemBuffer[2], mpb));
				auto D4 = static_cast<std::u8string*>(ar->GetData(TemBuffer[3], mpb));
				volatile int i = 0;
			}
		}

		volatile int iss = 0;
		*/
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