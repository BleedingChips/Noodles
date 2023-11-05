import std;
import NoodlesComponent;

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

	ArchetypeComponentManager Manager;

	auto entity = Manager.CreateEntityDefer(std::span(ids), [](EntityConstructor& cons)
	{
	});

	auto entity2 = Manager.CreateEntityDefer(std::span(ids), [](EntityConstructor& cons)
	{
	});

	//Manager.DestroyEntity(entity);

	Manager.UpdateEntityStatus();

	return 0;
}