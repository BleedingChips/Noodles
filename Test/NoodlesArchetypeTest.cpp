import std;
import NoodlesArchetype;

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

	ArchetypeConstructor con1;
	con1.AddElement(std::span(ids));

	auto ptr = Archetype::Create(
		con1
	);

	Tot temp;

	auto re_ad1 = ptr->GetData(0, 0, {&temp});
	static_cast<A*>(re_ad1)->i = 10086;

	auto re_ad2 = ptr->GetData(1, 0, { &temp });
	static_cast<B*>(re_ad2)->i = 100;

	auto re_ad3 = ptr->GetData(2, 0, { &temp });
	static_cast<C*>(re_ad3)->i = 86;

	auto re_ad4 = ptr->LocateTypeID(UniqueTypeID::Create<D>());

	{
		std::vector<Noodles::ArchetypeID> ids{
			ArchetypeID::Create<E>(),
			ArchetypeID::Create<E>()
		};

		ArchetypeConstructor con1;
		con1.AddElement(std::span(ids));

		auto ptr2 = Archetype::Create(con1);

		volatile int i = 0;
	}

	return 0;
}