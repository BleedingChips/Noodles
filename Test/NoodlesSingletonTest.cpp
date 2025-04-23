import std;

import Potato;

import NoodlesBitFlag;
import NoodlesClassBitFlag;
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

	AsynClassBitFlagMap map;

	SingletonManager manager{map.GetBitFlagContainerElementCount()};
	SingletonModifyManager modify{map.GetBitFlagContainerElementCount()};

	modify.AddSingleton(A{ 100861 }, map);
	modify.AddSingleton(B{ 100862 }, map);
	modify.AddSingleton(C{ 100863 }, map);
	modify.AddSingleton(D{ 100864 }, map);

	modify.FlushSingletonModify(manager);

	std::array<BitFlag, 3> info = {*map.LocateOrAdd<A>(), *map.LocateOrAdd<B>(), *map.LocateOrAdd<Report>()};
	std::array<std::size_t, SingletonManager::GetQueryDataCount() * 3> query_data;
	std::array<void*, 3> data;

	manager.TranslateBitFlagToQueryData(info, query_data);
	manager.QuerySingletonData(query_data, data);

	A* p1 = static_cast<A*>(data[0]);
	B* p2 = static_cast<B*>(data[1]);
	Report* p3 = static_cast<Report*>(data[2]);

	return 0;
}