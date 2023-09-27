import std;
import PotatoTaskSystem;
import NoodlesContext;

using namespace Noodles;

std::mutex PrintMutex;

void UniquePrint(std::u8string_view Name)
{
	{
		std::lock_guard lg(PrintMutex);
		std::println("Begin Func : {0}", Name);
	}

	std::this_thread::sleep_for(std::chrono::milliseconds{200});

	{
		std::lock_guard lg(PrintMutex);
		std::println("End Func : {0}", Name);
	}
}




int main()
{
	auto TSystem = Potato::Task::TaskContext::Create();
	TSystem->FireThreads();

	auto NContext = Context::Create({}, TSystem);

	/*
	void (*sysfunc)(void* object, ExecuteStatus & status),
		void* object,
		std::span<SystemRWInfo const> Infos,
		SystemProperty system_property,
		std::partial_ordering(*priority_detect)(void* Object, SystemProperty const&, SystemProperty const&)
	*/



	NContext->AddRawSystem(
		[](void* object, Noodles::Context::ExecuteStatus& status)
		{
			UniquePrint(status.property.system_name);
		},
		nullptr,
		nullptr,
		{},
		{
			SystemProperty::Category::Tick,
			*Potato::Task::TaskPriority::Normal,
			0, 0, {}, u8"Test Lambda1"
		},
		nullptr
	);

	/*
	std::chrono::time_zone tz{
				std::string_view{"Asia/BeiJing"}
	};
	auto cur = std::chrono::system_clock::now();
	//cur.
	auto ltime = std::chrono::current_zone()->to_local(cur);

	std::cout << typeid(decltype(ltime)).name() << std::endl;

	auto day = std::chrono::floor<std::chrono::days>(ltime);


	auto ymd = std::chrono::year_month_day {day};

	ymd.day()
	*/

	//zt.get_local_time();

	//auto dur = std::chrono::duration_cast<std::chrono::year>(ltime - zt.get_local_time());

	//auto lday = std::chrono::time_point_cast<std::chrono::local_seconds::duration>(ltime);

	//std::chrono::hh_mm_ss dur = ltime;

	//std::cout << ltime << std::endl;

	//std::chrono::local_days ld{ltime};


	/*
	EntityPolicy Policy;

	auto Entity = NContext->CreateEntity(Policy);
	*/

	return 0;
}