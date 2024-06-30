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
	{
		TemporaryComponentFilterStorage<Report, A, EntityProperty, std::u8string> TF;
		//TestComponentFilter<Report, A, EntityProperty, std::u8string> TF;
		//TestSingletonFilter<A> ATF;

		ArchetypeComponentManager manager;

		auto e1 = manager.CreateEntity();

		manager.AddEntityComponent(*e1, Report{});
		manager.AddEntityComponent(*e1, A{ 10086 });
		manager.AddEntityComponent(*e1, std::u8string{u8"Fuck You"});

		for(std::size_t i = 0; i < 1000; ++i)
		{
			auto ite_e2 = manager.CreateEntity();
			manager.AddEntityComponent(*ite_e2, Report{});
			manager.AddEntityComponent(*ite_e2, A{ i });
			manager.AddEntityComponent(*ite_e2, std::u8string{u8"Fuck You"});
		}

		{
			A a{100};
			auto K = manager.MoveAndCreateSingleton(a);
		}
		

		manager.ForceUpdateState();

		auto e2 = manager.CreateEntity();

		manager.AddEntityComponent(*e2, Report{});
		manager.AddEntityComponent(*e2, A{ 1008611 });
		manager.AddEntityComponent(*e2, std::u8string{u8"Fuck You"});

		manager.ReleaseEntity(*e2);

		manager.ForceUpdateState();

		auto F1 = manager.CreateComponentFilter(TF);
		
		auto P = manager.ReadComponents_AssumedLocked(*F1, 0, TF);
		if(P)
		{
			auto k = P.GetRawArray(1).Translate<A>();
			auto k2 = P.GetRawArray(3).Translate<std::u8string>();

			auto K333 = std::move(k2[3]);

			auto k4 = P.GetRawArray(2).Translate<EntityProperty>();

			auto k5 = k4[0].GetEntity();


			manager.ReleaseEntity(*k5);

			manager.ForceUpdateState();

			auto P2 = manager.ReadComponents_AssumedLocked(*F1, 0, TF);
			if(P2)
			{
				auto k = P2.GetRawArray(1).Translate<A>();
				auto k55 = P2.GetRawArray(2).Translate<EntityProperty>();

				manager.ReleaseEntity(*k55[1].GetEntity());

				auto ec2 = manager.CreateEntity();

				manager.AddEntityComponent(*ec2, Report{});
				manager.AddEntityComponent(*ec2, A{ 187888 });
				manager.AddEntityComponent(*ec2, std::u8string{u8"Fuck You"});

				manager.ForceUpdateState();

				auto P3 = manager.ReadComponents_AssumedLocked(*F1, 0, TF);

				if(P3)
				{
					auto k = P3.GetRawArray(1).Translate<A>();
					auto k55 = P3.GetRawArray(2).Translate<EntityProperty>();
					volatile int op = 0;
				}

				volatile int op = 0;
			}
			volatile int i = 0;
		}
	}


	volatile int i = 0;

	return 0;
}