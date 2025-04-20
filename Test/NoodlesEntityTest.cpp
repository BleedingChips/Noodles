import std;

import NoodlesEntity;
import NoodlesComponent;
import NoodlesClassBitFlag;
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


int main()
{
	AsynClassBitFlagMap map;

	ComponentManager c_manager{};
	EntityManager e_manager{ map};

	auto entity = e_manager.CreateEntity();
	auto entity2 = e_manager.CreateEntity();

	e_manager.AddEntityComponent(*entity, A{100861}, *map.LocateOrAdd<A>());
	e_manager.AddEntityComponent(*entity2, A{ 100862 }, *map.LocateOrAdd<A>());

	e_manager.FlushEntityModify(c_manager);

	std::array<BitFlag, 2> pt = {
		*map.LocateOrAdd<EntityProperty>(),
		* map.LocateOrAdd<A>(),
	};

	std::array<std::size_t, pt.size() * ComponentManager::GetQueryDataCount()> offset;

	std::array<void*, 2> output;

	c_manager.TranslateClassToQueryData(0, std::span(pt), offset);
	auto index = c_manager.QueryComponentArray(0, 0, offset, output);

	auto s1 = std::span{static_cast<EntityProperty*>(output[0]), index.Get()};
	auto s2 = std::span{ static_cast<A*>(output[1]), index.Get() };


	e_manager.ReleaseEntity(*entity);

	e_manager.FlushEntityModify(c_manager);

	index = c_manager.QueryComponentArray(0, 0, offset, output);

	auto s3 = std::span{ static_cast<EntityProperty*>(output[0]), index.Get() };
	auto s4 = std::span{ static_cast<A*>(output[1]), index.Get() };

	return 0;
}