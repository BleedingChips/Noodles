import std;

import NoodlesEntity;

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
	ComponentManager component_manager;
	EntityManager entity_manager;

	auto init_list2 = std::array<ComponentFilter::Info, 2>
	{
		ComponentFilter::Info{true, Potato::IR::StaticAtomicStructLayout<A>::Create()},
		ComponentFilter::Info{false, Potato::IR::StaticAtomicStructLayout<B>::Create()}
	};

	auto refuse_component = std::array<StructLayout::Ptr, 1>
	{
		StructLayout::GetStatic<E>()
	};

	
	entity_manager.Init(component_manager);

	auto filter = component_manager.CreateComponentFilter(
		init_list2,
		{},
		0
	);

	auto entity = entity_manager.CreateEntity(component_manager);

	auto re1 = entity_manager.AddEntityComponent(
		component_manager,
		entity, A{10086}
	);

	auto re2 = entity_manager.AddEntityComponent(
		component_manager,
		entity, B{ 10076 }
	);

	entity_manager.Flush(component_manager);

	auto k = component_manager.ReadComponentRow_AssumedLocked(*filter, 0);

	auto span_a = k->AsSpan<A>(0);
	auto span_b = k->AsSpan<B>(1);

	return 0;
}