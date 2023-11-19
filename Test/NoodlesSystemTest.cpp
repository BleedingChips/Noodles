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
	UniquePrint(context.GetProperty().system_name);
}

std::partial_ordering CustomPriority(SystemProperty const& p1, SystemProperty const& p2)
{
	return p1.system_name <=> p2.system_name;
}

struct A{ std::size_t index = 0; };

struct B{};


int main()
{


	ArchetypeComponentManager manager;

	Noodles::TickSystemsGroup group;

	EntityConstructor ec;
	ec.MoveConstruct(A{100});
	ec.MoveConstruct(A{ 99 });

	auto en = manager.CreateEntityDefer(ec);
	
	EntityConstructor ec2;
	ec2.MoveConstruct(A{ 98 });
	ec2.MoveConstruct(A{ 97 });
	ec2.MoveConstruct(A{ 96 });

	auto en2 = manager.CreateEntityDefer(ec2);

	manager.UpdateEntityStatus();


	struct Text
	{
		SystemComponentFilter::Ptr TopWrapper;
	};

	auto i1 = group.RegisterDefer(
		manager, std::numeric_limits<std::int32_t>::min(), SystemPriority{}, SystemProperty{u8"start"},
		[](FilterGenerator& Generator) -> std::size_t
		{
			return 0;
		},
		[](SystemContext& context, std::size_t)
		{
			PrintMark(context);
		}
	);

	auto i2 = group.RegisterDefer(
		manager, std::numeric_limits<std::int32_t>::max(), SystemPriority{}, SystemProperty{ u8"end" },
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

	auto i3 = group.RegisterDefer(
		manager, 0, default_pri, SystemProperty{ u8"S1" },
		[](FilterGenerator& Generator) -> std::size_t
		{
			std::vector<SystemRWInfo> infos ={
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

	auto i4 = group.RegisterDefer(
		manager, 0, default_pri, SystemProperty{ u8"S2" },
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

	struct Contr
	{
		SystemComponentFilter::Ptr filter;
		SystemEntityFilter::Ptr efilter;
	};

	auto i5 = group.RegisterDefer(
		manager, 0, default_pri, SystemProperty{ u8"S3" },
		[](FilterGenerator& Generator) -> Contr
		{
			std::vector<SystemRWInfo> infos = {
				SystemRWInfo::Create<A>()
			};
			auto fil = Generator.CreateComponentFilter(infos);
			auto k2 = Generator.CreateEntityFilter(infos);
			return {std::move(fil), std::move(k2)};
		},
		[&](SystemContext& context, Contr& C)
		{
			UniquePrint(context.GetProperty().system_name);
			if(context.GetSystemCategory() == SystemCatergory::Normal)
			{
				context.StartParallel(10);
			}else if(context.GetSystemCategory() == SystemCatergory::FinalParallel)
			{
				C.filter->Foreach(manager, [](SystemComponentFilter::Wrapper wra)
				{
					auto s = wra.Write<A>(0);
					return true;
				});

				C.efilter->ForeachEntity(manager, *en2, [](SystemEntityFilter::Wrapper wra)
				{
					auto s = wra.Write<A>(0);
					return true;
				});
			}
		}
	);






	while (true)
	{
		std::vector<TickSystemRunningIndex> ptrs;
		group.SynFlushAndDispatch(manager, [&](TickSystemRunningIndex ptr, std::u8string_view str)
		{
			ptrs.emplace_back(std::move(ptr));
		});

		while(!ptrs.empty())
		{
			auto top = std::move(*ptrs.rbegin());
			ptrs.pop_back();
			auto ite = group.ExecuteAndDispatchDependence(
				top, manager, (*static_cast<Context*>(nullptr)), [&](TickSystemRunningIndex ptr, std::u8string_view str)
				{
					ptrs.emplace_back(std::move(ptr));
				}
			);
			volatile int i = 0;
		}

		std::this_thread::sleep_for(std::chrono::seconds{5});

	}

	return 0;
}