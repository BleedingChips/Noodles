import std;
import PotatoTaskSystem;
import NoodlesContext;

using namespace Noodles;

std::mutex PrintMutex;

void PrintMark(Noodles::SystemContext& context)
{
	std::lock_guard lg(PrintMutex);
	std::println("---{0}---", std::string_view{
		reinterpret_cast<char const*>(context.GetProperty().system_name.data()),
		context.GetProperty().system_name.size()
		});
}

void UniquePrint(std::u8string_view Name, std::chrono::system_clock::duration dua = std::chrono::milliseconds{ 1 })
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
	UniquePrint(context.GetProperty().system_name);
}

std::partial_ordering CustomPriority(SystemProperty const& p1, SystemProperty const& p2)
{
	return p1.system_name <=> p2.system_name;
}

struct A { std::size_t i = 0; };

struct B {};


int main()
{


	auto task_context = Potato::Task::TaskContext::Create();
	task_context->FireThreads();


	ContextConfig config{
	1000,
		std::chrono::seconds{10}
	};

	auto context = Context::Create(
		config, task_context, u8"Fuck"
	);

	EntityConstructor ec;
	ec.MoveConstruct(A{ 100 });
	ec.MoveConstruct(A{ 99 });

	auto en = context->CreateEntityDefer(ec);

	EntityConstructor ec2;
	ec2.MoveConstruct(A{ 98 });
	ec2.MoveConstruct(A{ 97 });
	ec2.MoveConstruct(A{ 96 });

	auto en2 = context->CreateEntityDefer(ec2);

	struct Text
	{
		SystemComponentFilter::Ptr TopWrapper;
	};

	auto i1 = context->RegisterTickSystemDefer(
		std::numeric_limits<std::int32_t>::min(), SystemPriority{}, SystemProperty{ u8"start" },
		[](FilterGenerator& Generator) -> std::size_t
		{
			return 0;
		},
		[](SystemContext& context, std::size_t)
		{
			PrintMark(context);
		}
	);

	auto i2 = context->RegisterTickSystemDefer(
		std::numeric_limits<std::int32_t>::max(), SystemPriority{}, SystemProperty{ u8"end" },
		[](FilterGenerator& Generator) -> std::size_t
		{
			return 0;
		},
		[](SystemContext& context, std::size_t)
		{
			PrintMark(context);
		}
	);

	SystemPriority default_pri
	{
		0, 0, CustomPriority
	};

	auto i3 = context->RegisterTickSystemDefer(
		0, default_pri, SystemProperty{ u8"S1" },
		[](FilterGenerator& Generator) -> std::size_t
		{
			std::vector<SystemRWInfo> infos = {
				SystemRWInfo::Create<A const>()
			};
			Generator.CreateComponentFilter(infos);
			return 0;
		},
		[](SystemContext& context, std::size_t)
		{
			UniquePrint(context.GetProperty().system_name);
		}
	);

	int count = 30;

	auto i4 = context->RegisterTickSystemDefer(
		 0, default_pri, SystemProperty{ u8"S2" },
		[](FilterGenerator& Generator) -> std::size_t
		{
			std::vector<SystemRWInfo> infos = {
				SystemRWInfo::Create<A const>()
			};
			Generator.CreateComponentFilter(infos);
			return 0;
		},
		[&](SystemContext& context, std::size_t)
		{
			UniquePrint(context.GetProperty().system_name);
			count -= 1;
			if(count == 0)
			{
				context->RequireExist();
			}
		}
	);

	struct Contr
	{
		SystemComponentFilter::Ptr filter;
		SystemEntityFilter::Ptr efilter;
	};

	auto i5 = context->RegisterTickSystemDefer(
		 0, default_pri, SystemProperty{ u8"S3" },
		[](FilterGenerator& Generator) -> Contr
		{
			std::vector<SystemRWInfo> infos = {
				SystemRWInfo::Create<A>()
			};
			auto fil = Generator.CreateComponentFilter(infos);
			auto k2 = Generator.CreateEntityFilter(infos);
			return { std::move(fil), std::move(k2) };
		},
		[&](SystemContext& sys_context, Contr const& C)
		{
			UniquePrint(sys_context.GetProperty().system_name);
			if (sys_context.GetSystemCategory() == SystemCatergory::Normal)
			{
				sys_context->StartSelfParallel(sys_context, 10);
			}
			else if (sys_context.GetSystemCategory() == SystemCatergory::FinalParallel)
			{
				sys_context->Foreach(*C.filter, [](SystemComponentFilter::Wrapper wra)
					{
						auto s = wra.Write<A>(0);
						return true;
					});

				sys_context->ForeachEntity(*C.efilter, *en2, [](SystemEntityFilter::Wrapper wra)
					{
						auto s = wra.Write<A>(0);
						return true;
					});
			}
		}
	);

	context->StartLoop();

	task_context->FlushTask();

	return 0;
}