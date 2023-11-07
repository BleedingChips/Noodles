import std;
import PotatoTaskSystem;
import NoodlesContext;

using namespace Noodles;

std::mutex PrintMutex;

void PrintMark(Noodles::SystemContext& context)
{
	std::lock_guard lg(PrintMutex);
	std::println("---{0}---", std::string_view{
		reinterpret_cast<char const*>(context.self_property.system_name.data()),
		context.self_property.system_name.size()
	});
}

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

void PrintSystem(Noodles::SystemContext& context)
{
	UniquePrint(context.self_property.system_name);
}

struct A{};

struct B{};


int main()
{


	ArchetypeComponentManager Manager;

	Noodles::TickSystemsGroup Tg;

	Tg.LoginDefer(
	);


	Tg.RegisterDefer(
		0, SystemPriority{}, SystemProperty{u8"balabala"},
		[](FilterGenerator& Generator)-> FilterGenerator
		{
			std::vector<SystemRWInfo> infos;
			Generator.AddComponentFilter(
				std::span(infos)
			);

		},
		[](SystemContext& context)
		{
			auto component_filer = context.GetComponentFilter(0);
		}
	);


	Tg.LoginDefer(
		0, SystemPriority{}, SystemProperty{u8"balabala"}, [](SystemContext& context){}
	);





	/*
	auto TSystem = Potato::Task::TaskContext::Create();
	TSystem->FireThreads();

	Noodles::ContextConfig config;
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

	auto r1 = NContext->AddTickSystemDefer(
		{
			std::numeric_limits<std::int32_t>::max()
		},
		{
			u8"start"
		},
		{
		},
		PrintMark
	);

	auto r2 = NContext->AddTickSystemDefer(
		{
			std::numeric_limits<std::int32_t>::min()
		},
		{
			u8"end"
		},
		{
		},
		PrintMark
	);

	auto r3 = NContext->AddTickSystemDefer(
		{
			0,0,0,
		},
		{
			u8"S1"
		},
		{
				std::span(rw_infos1)
		},
		PrintSystem
	);

	auto r4 = NContext->AddTickSystemDefer(
		{
			0,0,1,
		},
			{
					u8"S2"
			},
			{
				std::span(rw_infos1)
			},
		PrintSystem
	);

	auto r5 = NContext->AddTickSystemDefer(
		{
			0,0,2,
		},
			{
					u8"S3"
			},
			{
				std::span(rw_infos1)
			},
			PrintSystem
	);

	auto r6 = NContext->AddTickSystemDefer(
		{
			0,0,3,
		},
			{
					u8"S4"
			},
			{
				std::span(rw_infos1)
			},
			[](ExecuteContext& context)
			{
				UniquePrint(context.property.system_name);
			}
	);

	auto r7 = NContext->AddTickSystemDefer(
		{
			0,0,4,
		},
			{
					u8"S5"
			},
			{
				std::span(rw_infos1)
			},
		[]()//ExecuteContext& context, ComponentFilter<A>)
		{
			std::lock_guard lg(PrintMutex);
			std::println("Funck You !");
		}
	);
	

	NContext->StartLoop();
	TSystem->WaitTask();
	*/

	return 0;
}