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


int main()
{

	std::vector<Noodles::ArchetypeID> ids{
		ArchetypeID::Create<A>(),
		ArchetypeID::Create<B>(),
		ArchetypeID::Create<C>()
	};

	auto ptr = Archetype::Create(
		std::span(ids)
	);

	Tot temp;

	auto re_ad1 = ptr->GetData(0, {&temp});
	static_cast<A*>(re_ad1)->i = 10086;

	auto re_ad2 = ptr->GetData(1, { &temp });
	static_cast<B*>(re_ad2)->i = 100;

	auto re_ad3 = ptr->GetData(2, { &temp });
	static_cast<C*>(re_ad3)->i = 86;

	auto re_ad4 = ptr->LocateFirstTypeID(UniqueTypeID::Create<D>());

	return 0;
}