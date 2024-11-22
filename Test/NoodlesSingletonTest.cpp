import std;

import NoodlesSingleton;

import PotatoIR;

using namespace Noodles;

struct alignas(128) A { std::size_t i = 0; };

struct alignas(64) B { std::size_t i = 1; };

struct alignas(32) C { std::size_t i = 1; };

struct alignas(32) D { std::size_t i = 1; };

static std::int32_t index = 0;

struct alignas(8) Report {
	Report() { index += 1; }
	Report(Report const&) { index += 1; }
	Report(Report&&) { index += 1; }
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
	auto init_list = std::array{
				Potato::IR::StaticAtomicStructLayout<B>::Create(),
				Potato::IR::StaticAtomicStructLayout<A>::Create(),
				Potato::IR::StaticAtomicStructLayout<C>::Create(),
				Potato::IR::StaticAtomicStructLayout<D>::Create(),
				Potato::IR::StaticAtomicStructLayout<E>::Create()
	};

	auto init_list2 = std::array<SingletonFilter::Info, 2>
	{
		SingletonFilter::Info{true, Potato::IR::StaticAtomicStructLayout<A>::Create()},
		SingletonFilter::Info{false, Potato::IR::StaticAtomicStructLayout<B>::Create()}
	};

	auto refuse_component = std::array<StructLayout::Ptr, 1>
	{
		StructLayout::GetStatic<E>()
	};


	SingletonManager singleton_manager;

	auto filter = singleton_manager.CreateSingletonFilter(
		init_list2,
		{}
	);

	singleton_manager.AddSingleton(A{10086});
	singleton_manager.AddSingleton(B{ 10071 });

	auto k = singleton_manager.ReadSingleton_AssumedLocked(*filter);

	auto a = k.As<A>(0);
	auto b = k.As<B>(1);

	singleton_manager.Flush();

	auto k2 = singleton_manager.ReadSingleton_AssumedLocked(*filter);

	auto a2 = k.As<A>(0);
	auto b2 = k.As<B>(1);

	auto a3 = k2.As<A>(0);
	auto b3 = k2.As<B>(1);

	return 0;
}