import std;
import PotatoTaskSystem;
import NoodlesComponent;
import NoodlesContext;

using namespace Noodles;

std::mutex PrintMutex;

/*
void PrintMark(Noodles::SystemContext& context)
{
	std::lock_guard lg(PrintMutex);
	std::println("---{0}---", std::string_view{
		reinterpret_cast<char const*>(context.GetProperty().system_name.data()),
		context.GetProperty().system_name.size()
		});
}

void UniquePrint(std::u8string_view Name, std::chrono::system_clock::duration dua = std::chrono::milliseconds{ 1000 })
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

void Func(A a){}

void Func2(SystemContext system) {}

void Func3(SystemContext& context, ComponentFilter<A, B> system, ComponentFilter<A, B> system2, std::size_t& I)
{
	I = 10086;
	volatile int i = 0;
}
*/

struct Tuple
{
	std::size_t i = 0;
};

struct Tuple2
{
	std::u8string str;
};

int main()
{

	Context::Config fig;
	fig.min_frame_time = std::chrono::seconds{ 1 };

	Context context{fig};

	Potato::Task::TaskContext tcontext;

	auto ent = context.CreateEntityDefer(Tuple{ 10086 });

	for(std::size_t o = 0; o < 100; ++o)
	{
		context.CreateEntityDefer(Tuple{ o });
	}

	auto Ker = context.CreateSingleton<Tuple2>(std::u8string{u8"Fff"});

	context.CreateTickSystemAuto( {0, 0, 1}, {
		u8"wtf1"
	}, [=](ExecuteContext& context,  ComponentFilter<Tuple const, EntityProperty>& p, SingletonFilter<Tuple2>& s, std::size_t i)
	{
		ComponentFilter<Tuple const, EntityProperty>::OutputIndexT output;
		auto k = p.IterateComponent(context,0, output);

		auto L = p.GetByIndex<0>(k);
		auto O = p.GetByIndex<1>(k);
		auto K = p.GetByType<Tuple const>(k);

		auto k2 = p.ReadEntity(context, *ent, output);

		auto ik = s.Get(context);

		auto I = p.GetByIndex<0>( k2);

			std::println("wtf1");
			
	});

	std::size_t index = 0;

	context.CreateTickSystemAuto({0, 0, 3}, {
		u8"wtf2"
		}, [&](ExecuteContext& context, ComponentFilter<Tuple, EntityProperty>& p, std::size_t i)
		{
			std::println("wtf2");

			index += 1;
			if(index == 5)
			{
				context.noodles_context.RemoveSystemDefer({u8"wtf1"});
			}

			if(index == 5)
			{
				context.noodles_context.CreateTickSystemAuto(
					{0, 0, 1}, {u8"wtf3"}, [](ExecuteContext& context, ComponentFilter<Tuple, EntityProperty>& p)
					{
						std::println("wtf3");
					}
				);
			}

			volatile int i22 = 0;
		});

	context.FlushStats();

	bool re = context.Commit(tcontext, {});
	tcontext.ProcessTaskUntillNoExitsTask({});


	/*
	//static_assert(IsAcceptableFunctionT<decltype(Func3)>::value, "Func");

	//using K = typename ExtractAppendData<decltype(Func3)>::Type;

	//static_assert(!std::is_same_v<K, std::tuple<SystemComponentFilter::Ptr>>, "Fuck");


	Potato::Task::TaskContext task_context;


	//auto task_context = Potato::Task::TaskContext::Create();

	task_context.AddGroupThread({}, Potato::Task::TaskContext::GetSuggestThreadCount());

	ContextConfig config;

	config.min_frame_time = std::chrono::seconds{10};

	auto context = Context::Create(
		config, u8"Fuck"
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
			if (sys_context.GetSystemCategory() == SystemCategory::Normal)
			{
				sys_context->StartSelfParallel(sys_context, 10);
			}
			else if (sys_context.GetSystemCategory() == SystemCategory::FinalParallel)
			{
				sys_context->Foreach(*C.filter, [](SystemComponentFilter::Wrapper wra)
					{
						auto s = wra.GetSpan<A>(0);
						return true;
					});

				sys_context->ForeachEntity(*C.efilter, *en2, [](SystemEntityFilter::Wrapper wra)
					{
						auto s = wra.Write<A>(0);
						return true;
					});
			}else if(sys_context.GetSystemCategory() == SystemCategory::Parallel)
			{
				auto index = sys_context.GetParallelIndex();
				volatile int i = 0;
				return;
			}
		}
	);


	auto i6 = context->RegisterTickSystemAutoDefer(
		0, default_pri, SystemProperty{ u8"S10" },
		[&](SystemContext& context, std::size_t, ComponentFilter<A const> fil, EntityFilter<A const> enf)
		{
			UniquePrint(context.GetProperty().system_name);
			context->Foreach(fil, [](std::span<A const> span1)-> bool
			{
				volatile int i = 0;
				return true;
			});
			context->ForeachEntity(enf, *en2, [](EntityStatus status, std::span<A const> span)->bool
			{
				return true;
			});
		}
	);



	context->StartLoop(task_context, {});

	task_context.ProcessTaskUntillNoExitsTask({});
	*/

	return 0;
}