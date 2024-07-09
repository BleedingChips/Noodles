import std;
import PotatoTaskSystem;
import NoodlesComponent;
import NoodlesContext;
import PotatoEncode;

using namespace Noodles;

std::mutex PrintMutex;

void PrintSystemProperty(ExecuteContext& context)
{
	auto wstr = *Potato::Encode::StrEncoder<char8_t, wchar_t>::EncodeToString(context.display_name);
	auto sstr = *Potato::Encode::StrEncoder<wchar_t, char>::EncodeToString(std::wstring_view{wstr});
	{
		std::lock_guard lg(PrintMutex);
		std::println("	StartSystemNode -- <{0}>", sstr);
	}
	std::this_thread::sleep_for(std::chrono::seconds{1});
	{
		std::lock_guard lg(PrintMutex);
		std::println("	EndSystemNode -- <{0}>", sstr);
	}
}


struct A { std::size_t i = 0; };

struct B {};

/*
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

struct TestContext : public Context
{
	using Context::Context;
	void AddContextRef() const override {}
	void SubContextRef() const override {}
};


struct TestSystem : public SystemNode
{
	void AddSystemNodeRef() const override {}
	void SubSystemNodeRef() const override {}
	void FlushMutexGenerator(ReadWriteMutexGenerator& generator) const override
	{
		static std::array<RWUniqueTypeID, 1> test = {
			RWUniqueTypeID::Create<Tuple2>()
		};
		generator.RegisterComponentMutex(std::span(test));
	}
	void SystemNodeExecute(ExecuteContext& context) override { PrintSystemProperty(context); }
};

void TestFunction(ExecuteContext& context, AtomicComponentFilter<Tuple2>& fup, AtomicSingletonFilter<Tuple2>& filter)
{
	auto P = filter.Get(context);
	PrintSystemProperty(context);
}


int main()
{
	//static_assert(std::is_function_v<decltype([](){})>);

	Context::Config fig;
	fig.min_frame_time = std::chrono::seconds{ 1 };

	TestContext context{fig};

	Potato::Task::TaskContext tcontext;
	TestSystem systm;

	auto ent = context.CreateEntity();
	context.AddEntityComponent(*ent, Tuple{10086});

	for(std::size_t o = 0; o < 100; ++o)
	{
		auto ent2 = context.CreateEntity();
		context.AddEntityComponent(*ent2, Tuple{o});
	}

	auto Ker = context.MoveAndCreateSingleton<Tuple2>(Tuple2{std::u8string{u8"Fff"}});
	context.AddSystem(&systm, {
		{1, 1, 1},
		{u8"S1", u8"G11"}
	});
	context.AddSystem(&systm, {
		{1, 1, 0},
		{u8"S2", u8"G11"}
	});
	context.AddSystem(&systm, {
		{2, 1, 1},
		{u8"S3", u8"G21"}
	});
	context.CreateAndAddAtomaticSystem(TestFunction, 
		{
		{1, 1, 3},
		{u8"S4", u8"G11"}
	}
		
		);

	/*
	context.CreateTickSystemAuto( {0, 0, 1}, {
		u8"wtf1"
	}, [=](ExecuteContext& context,  ComponentFilter<Tuple const, EntityProperty>& p, SingletonFilter<Tuple2>& s, std::size_t i)
	{
		ComponentFilter<Tuple const, EntityProperty>::OutputIndexT output;
		auto k = p.IterateComponent(context,0);

		auto L = p.GetByIndex<0>(k);
		auto O = p.GetByIndex<1>(k);
		auto K = p.GetByType<Tuple const>(k);

		auto k2 = p.ReadEntity(context, *ent);

		auto ik = s.Get(context);

		auto I = p.GetByIndex<0>( k2);

			std::println("wtf1");
			
	});
	*/

	std::size_t index = 0;

	/*
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
	*/
	tcontext.AddGroupThread({}, 2);
	bool re = context.Commited(tcontext, {});
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