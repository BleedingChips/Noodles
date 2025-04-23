import std;

import NoodlesEntity;
import NoodlesComponent;
import NoodlesSingleton;
import NoodlesClassBitFlag;
import NoodlesQuery;
import NoodlesBitFlag;


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


template<typename ...AT>
struct ValueData
{
	std::pmr::vector<BitFlagContainer::Element> container;
	ValueData(Noodles::AsynClassBitFlagMap& map)
	{
		container.resize(map.GetBitFlagContainerElementCount());

		auto init_list = { *map.LocateOrAdd<AT>()... };

		for (auto ite : init_list)
		{
			BitFlagContainer{container}.SetValue(ite);
		}
	}
	operator BitFlagContainerConstViewer () const { return std::span(container); }
};



int main()
{

	AsynClassBitFlagMap map;

	ComponentManager c_manager{};
	EntityManager e_manager{ map };

	auto entity = e_manager.CreateEntity();
	auto entity2 = e_manager.CreateEntity();

	e_manager.AddEntityComponent(*entity, A{ 100861 }, *map.LocateOrAdd<A>());
	e_manager.AddEntityComponent(*entity2, A{ 100862 }, *map.LocateOrAdd<A>());

	e_manager.FlushEntityModify(c_manager);

	std::array<BitFlag, 2> pt = {
		*map.LocateOrAdd<EntityProperty>(),
		*map.LocateOrAdd<A>(),
	};

	std::array<BitFlag, 1> pt2 = {
		*map.LocateOrAdd<A>()
	};

	std::array<void*, 2> output;

	auto query = ComponentQuery::Create(
		map.GetBitFlagContainerElementCount(), 
		c_manager.GetArchetypeBitFlagContainerCount(),
		pt,
		pt2,
		{}
	);
	
	query->UpdateQueryData(c_manager);

	auto index = query->QueryComponentArrayWithIterator(c_manager, 0, 0, output);

	auto s1 = std::span{ static_cast<EntityProperty*>(output[0]), *index };
	auto s2 = std::span{ static_cast<A*>(output[1]), *index };

	e_manager.ReleaseEntity(*entity);

	e_manager.FlushEntityModify(c_manager);

	index = query->QueryComponentArrayWithIterator(c_manager, 0, 0, output);

	auto s3 = std::span{ static_cast<EntityProperty*>(output[0]), *index };
	auto s4 = std::span{ static_cast<A*>(output[1]), *index };

	query->QueryComponent(c_manager, *entity2->GetEntityIndex(), output);

	auto s5 = std::span{ static_cast<EntityProperty*>(output[0]), 1 };
	auto s6 = std::span{ static_cast<A*>(output[1]), 1 };

	SingletonManager s_manager{ map.GetBitFlagContainerElementCount() };
	SingletonModifyManager m_manager{ map.GetBitFlagContainerElementCount() };

	m_manager.AddSingleton(A{ 10081 }, map);
	m_manager.AddSingleton(B{ 10082 }, map);
	m_manager.AddSingleton(C{ 10083 }, map);
	m_manager.AddSingleton(D{ 10084 }, map);

	m_manager.FlushSingletonModify(s_manager);

	std::array<BitFlag, 3> info = {*map.LocateOrAdd<A>(), *map.LocateOrAdd<C>(), *map.LocateOrAdd<Report>()};

	std::array<void*, 3> output2;

	auto s_query = SingletonQuery::Create(map.GetBitFlagContainerElementCount(), info);

	s_query->UpdateQueryData(s_manager);
	s_query->QuerySingleton(s_manager, output2);

	A* p1 = static_cast<A*>(output2[0]);
	C* p2 = static_cast<C*>(output2[1]);
	Report* p3 = static_cast<Report*>(output2[2]);

	return 0;

	/*
	ComponentManager c_manager{global_context};
	EntityManager e_manager{global_context};

	auto entity = e_manager.CreateEntity();
	auto entity2 = e_manager.CreateEntity();

	e_manager.AddEntityComponent(*entity, A{100861});
	e_manager.AddEntityComponent(*entity2, A{ 100862 });

	e_manager.FlushEntityModify(c_manager);

	std::array<BitFlag, 2> pt = {
		*global_context->GetComponentBitFlag(*Potato::IR::StaticAtomicStructLayout<EntityProperty>::Create()),
		*global_context->GetComponentBitFlag(*Potato::IR::StaticAtomicStructLayout<A>::Create()),
	};

	std::array<std::size_t, pt.size() * 2> offset;

	std::array<void*, 2> output;

	c_manager.TranslateClassToComponentOffsetAndSize(0, std::span(pt), offset);
	auto index = c_manager.QueryComponentArrayWithComponentOffsetAndSize(0, 0, offset, output);

	auto s1 = std::span{static_cast<EntityProperty*>(output[0]), index.Get()};
	auto s2 = std::span{ static_cast<A*>(output[1]), index.Get() };


	e_manager.ReleaseEntity(*entity);

	e_manager.FlushEntityModify(c_manager);

	index = c_manager.QueryComponentArrayWithComponentOffsetAndSize(0, 0, offset, output);

	auto s3 = std::span{ static_cast<EntityProperty*>(output[0]), index.Get() };
	auto s4 = std::span{ static_cast<A*>(output[1]), index.Get() };
	*/

	return 0;
}