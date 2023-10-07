import std;
import PotatoTaskSystem;
import NoodlesContext;

using namespace Noodles;

std::mutex PrintMutex;

void UniquePrint(std::u8string_view Name, std::chrono::system_clock::duration dua = std::chrono::milliseconds{ 200 })
{
	{
		std::lock_guard lg(PrintMutex);
		std::println("Begin Func : {0}", std::string_view{
			reinterpret_cast<char const*>(Name.data()),
			Name.size()
		});
	}

	std::this_thread::sleep_for(dua);

	{
		std::lock_guard lg(PrintMutex);
		std::println("End Func : {0}", std::string_view{
			reinterpret_cast<char const*>(Name.data()),
			Name.size()
		});
	}
}

struct A{};

struct B{};


int main()
{
	auto TSystem = Potato::Task::TaskContext::Create();
	TSystem->FireThreads();

	Noodles::Context::Config config;
	config.min_frame_time = std::chrono::seconds{10};

	auto NContext = Context::Create(config, TSystem);

	std::vector<Noodles::System::RWInfo> rw_infos1 ={
		{
			System::RWInfo::GetComponent<A>()
		}
	};

	std::vector<Noodles::System::RWInfo> rw_infos2 = {
		{
			System::RWInfo::GetComponent<A const>()
		}
	};

	/*
	void (*sysfunc)(void* object, ExecuteStatus & status),
		void* object,
		std::span<SystemRWInfo const> Infos,
		SystemProperty system_property,
		std::partial_ordering(*priority_detect)(void* Object, SystemProperty const&, SystemProperty const&)
	*/

	{
		Noodles::System::Object obj {
			[](void* object, Noodles::ExecuteContext& status)
			{
				UniquePrint(status.property.system_name, std::chrono::milliseconds { 400 });
			}
		};

		NContext->AddRawTickSystem(
			{},
			{
				u8"Test Lambda1"
			},
			{
				std::span(rw_infos1)
			},
			std::move(obj)
		);
	}

	
	{
		Noodles::System::Object obj{
			[](void* object, Noodles::ExecuteContext& status)
			{
				UniquePrint(status.property.system_name, std::chrono::milliseconds { 400 });
			}
		};

		NContext->AddRawTickSystem(
			{
				1, -1
			},
			{
				u8"Test Lambda2"
			},
			{
				std::span(rw_infos2)
			}, std::move(obj)
		);
	}
	

	NContext->StartLoop();
	TSystem->WaitTask();

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