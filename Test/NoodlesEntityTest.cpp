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
	auto manager = StructLayoutManager::Create();
	ComponentManager component_manager{ *manager };
	EntityManager entity_manager { *manager };

	auto init_list2 = std::array<StructLayoutWriteProperty, 2>
	{
		StructLayoutWriteProperty::Get<A>(),
		StructLayoutWriteProperty::Get<B const>()
	};

	auto refuse_component = std::array<StructLayout::Ptr, 1>
	{
		StructLayout::GetStatic<E>()
	};

	auto filter = ComponentFilter::Create(*manager, init_list2, {});
	auto filter2 = ComponentFilter::Create(*manager, init_list2, {});

	auto entity = entity_manager.CreateEntity();

	auto re1 = entity_manager.AddEntityComponent(
		*entity, A{10086}
	);

	auto re2 = entity_manager.AddEntityComponent(
		*entity, B{ 10076 }
	);

	auto entity2 = entity_manager.CreateEntity();

	auto re21 = entity_manager.AddEntityComponent(
		*entity2, A{ 10086 }
	);

	auto re22 = entity_manager.AddEntityComponent(
		*entity2, B{ 10076 }
	);

	entity_manager.Flush(component_manager);

	auto b1 = filter->IsIsOverlappingRunTime(*filter2, component_manager.GetArchetypeUsageMark_AssumedLocked());

	entity_manager.RemoveEntityComponent(
		*entity, *StructLayout::GetStatic<B>()
	);

	auto re3 = entity_manager.AddEntityComponent(
		*entity, C{ 10086 }
	);

	auto re4 = entity_manager.AddEntityComponent(
		*entity, D{ 10076 }
	);

	entity_manager.Flush(component_manager);
	component_manager.UpdateFilter_AssumedLocked(*filter);
	component_manager.UpdateFilter_AssumedLocked(*filter2);

	auto b2 = filter->IsIsOverlappingRunTime(*filter2, component_manager.GetArchetypeUsageMark_AssumedLocked());

	std::array<void*, 100> buffer;
	ComponentAccessor accessor(buffer);

	auto k = component_manager.ReadComponentRow_AssumedLocked(*filter, 0, accessor);

	if(k)
	{
		auto span_a = accessor.AsSpan<A>(0);
		auto span_b = accessor.AsSpan<B>(1);
		volatile int i = 0;
	}

	

	return 0;
}