import std;
import NoodlesArchetype;
import PotatoIR;

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

	StructLayoutMarkIndexManager manager;

	auto init_list = std::array{
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<A>::Create(), *manager.LocateOrAdd(StructLayout::GetStatic<A>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<B>::Create(), *manager.LocateOrAdd(StructLayout::GetStatic<B>())},
		Archetype::Init{Potato::IR::StaticAtomicStructLayout<C>::Create(), *manager.LocateOrAdd(StructLayout::GetStatic<C>())}
	};

	

	auto ptr = Archetype::Create(manager, std::span(init_list));

	Tot temp;

	auto a = reinterpret_cast<A*>(
		reinterpret_cast<std::byte*>(&temp) +
		ptr->GetMemberView({0}).offset
	);
	a->i = 10086;

	auto b = reinterpret_cast<B*>(
		reinterpret_cast<std::byte*>(&temp) +
		ptr->GetMemberView({ 1 }).offset
		);
	b->i = 100;

	auto c = reinterpret_cast<C*>(
		reinterpret_cast<std::byte*>(&temp) +
		ptr->GetMemberView({ 2 }).offset
		);
	c->i = 86;

	auto re_ad4 = ptr->Locate(*manager.LocateOrAdd(StructLayout::GetStatic<D>()));

	return 0;
}