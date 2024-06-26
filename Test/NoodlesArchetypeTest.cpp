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

	std::vector<Noodles::AtomicType::Ptr> ids{
		GetAtomicType<A>(),
		GetAtomicType<B>(),
		GetAtomicType<C>()
	};

	auto ptr = Archetype::Create(std::span(ids));

	Tot temp;

	auto re_ad1 = ptr->Get((*ptr)[0], {&temp, 1, 1}, 0);
	static_cast<A*>(re_ad1)->i = 10086;

	auto re_ad2 = ptr->Get((*ptr)[1], { &temp, 1, 1 }, 0);
	static_cast<B*>(re_ad2)->i = 100;

	auto re_ad3 = ptr->Get((*ptr)[2], { &temp, 1, 1 }, 0);
	static_cast<C*>(re_ad3)->i = 86;

	auto re_ad4 = ptr->LocateTypeID(*GetAtomicType<D>());

	{
		std::vector<AtomicType::Ptr> ids{
			GetAtomicType<E>(),
			GetAtomicType<E>()
		};

		auto ptr2 = Archetype::Create(std::span(ids));

		volatile int i = 0;
	}


	{
		std::vector<AtomicType::Ptr> ids{
			GetAtomicType<D>(),
			GetAtomicType<D>()
		};

		auto ptr2 = Archetype::Create(std::span(ids));

		volatile int i = 0;
	}

	return 0;
}