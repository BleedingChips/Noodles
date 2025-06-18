import std;
import Potato;
import Noodles;

struct A { std::size_t i = 0; };

struct B { std::size_t i[2] = {1, 2}; };

struct C { std::size_t i[2] = { 1, 2 }; };


/*
std::mutex PrintMutex;

void PrintSystemProperty(Noodles::ContextWrapper& wrapper)
{
	auto wstr = *Potato::Encode::StrEncoder<char8_t, wchar_t>::EncodeToString(wrapper.GetNodeProperty().node_name);
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

void Func(A a){}

void Func2(SystemContext system) {}

void Func3(SystemContext& context, ComponentFilter<A, B> system, ComponentFilter<A, B> system2, std::size_t& I)
{
	I = 10086;
	volatile int i = 0;
}
*/

struct SysNode : public Noodles::SystemNode
{
	virtual void SystemNodeExecute(Noodles::Context& context) {

		std::array<Noodles::ComponentQuery::OPtr, 10> out_comp;
		std::array<Noodles::SingletonQuery::OPtr, 10> out_sing;

		auto [s, p] = context.GetQuery(out_comp, out_sing);
		
		std::this_thread::sleep_for(std::chrono::seconds{1});
	}

	virtual void Init(Noodles::SystemInitializer& initlizer) override
	{
		
		initlizer.CreateComponentQuery(2, [](Noodles::ComponentQueryInitializer& comp_init) {
			comp_init.SetRequire<A const>();
			comp_init.SetRequire<B>();
		})
		;

		/*
		initlizer.CreateSingletonQuery(2, [](Noodles::SingletonQueryInitializer& sing_init) {
			sing_init.SetRequire<A const>();
			sing_init.SetRequire<B>();
		});
		*/
	}


	virtual void AddSystemNodeRef() const {}
	virtual void SubSystemNodeRef() const {}
}test_sys;

struct Tuple
{
	std::size_t i = 0;
};

struct Tuple2
{
	std::u8string str;
};

/*
struct TestContext : public Noodles::Context
{
	TestContext(Noodles::StructLayoutManager& manager,  Config config) : Context(manager, config) {}
protected:
	void AddContextRef() const override {}
	void SubContextRef() const override {}
	void TaskFlowExecuteBegin_AssumedLocked(Potato::Task::ContextWrapper& wrapper) override
	{
		Context::TaskFlowExecuteBegin_AssumedLocked(wrapper);
		std::println("Context Begin---");
	}
};



struct TestSystem : public Noodles::SystemNode
{
	void AddSystemNodeRef() const override {}
	void SubSystemNodeRef() const override {}
	void SystemNodeExecute(Noodles::ContextWrapper& context) override { PrintSystemProperty(context); }
};

void TestFunction(Noodles::ContextWrapper& wrapper, Noodles::AutoComponentQuery<Tuple2> component_query, Noodles::AutoSingletonQuery<Tuple2> singleton_query)
{
	component_query.IterateComponent(wrapper, 0);
	singleton_query.GetSingletons(wrapper);
	auto P = component_query.AsSpan<0>();
	PrintSystemProperty(wrapper);
}
*/

int main()
{
	auto sys = Noodles::CreateAutoSystemNode([](
		Noodles::Context& context, 
		Noodles::AutoComponentQuery<A>::template Refuse<C> query,
		Noodles::AutoSingletonQuery<A> s_query
		) {
		volatile int  i = 0;
	});


	{
		Potato::Task::Context context;
		auto instance = Noodles::Instance::Create();
		auto sys_index = instance->PrepareSystemNode(sys);
		auto s1 = instance->PrepareSystemNode(&test_sys);
		//auto s2 = instance->PrepareSystemNode(&test_sys);
		Noodles::SystemNode::Parameter sys_par;
		sys_par.name = L"TestSystem1!!";
		sys_par.layer = -1;
		instance->LoadSystemNode(Noodles::SystemCategory::Tick, s1, sys_par);
		sys_par.name = L"TestSystem2!!";
		instance->LoadSystemNode(Noodles::SystemCategory::Tick, s1, sys_par);

		sys_par.name = L"TestSystem3!!";
		instance->LoadSystemNode(Noodles::SystemCategory::OnceNextFrame, s1, sys_par);
		sys_par.name = L"TestSystem4!!";
		instance->LoadSystemNode(Noodles::SystemCategory::OnceNextFrame, s1, sys_par);

		sys_par.name = L"TestSystem66!!";
		instance->LoadSystemNode(Noodles::SystemCategory::Tick, sys_index, sys_par);
		Noodles::Instance::Parameter par;
		par.duration_time = std::chrono::milliseconds{ 3000 };
		auto enti = instance->CreateEntity();
		instance->AddEntityComponent(*enti, A{ 10086 });
		instance->AddEntityComponent(*enti, B{ 100862 });
		instance->Commit(context, par);
		//context.CreateThreads(2);
		context.ExecuteContextThreadUntilNoExistTask();
	}
	



	//static_assert(std::is_function_v<decltype([](){})>);
	/*
	Noodles::Context::Config fig;
	fig.min_frame_time = std::chrono::seconds{ 1 };

	auto manager = Noodles::StructLayoutManager::Create();

	TestContext context{*manager, fig};

	Potato::Task::Context tcontext;
	TestSystem systm;

	auto ent = context.CreateEntity();
	context.AddEntityComponent(*ent, Tuple{10086});
	context.AddEntityComponent(*ent, Tuple2{ u8"" });

	for(std::size_t o = 0; o < 100; ++o)
	{
		auto ent2 = context.CreateEntity();
		context.AddEntityComponent(*ent2, Tuple{o});
	}

	auto Lambda = [](Noodles::ContextWrapper& context, Noodles::AutoComponentQuery<Tuple2> query)
	{
		PrintSystemProperty(context);
	};

	auto Ker = context.AddSingleton(Tuple2{ std::u8string{u8"Fff"} });


	auto b1 = Noodles::CreateAndAddAutomaticSystem(context, Lambda,
		Noodles::Property{
			{1, 1, 1},
			{u8"fun1 1_1_1"}
		}
	);
	auto b2 = Noodles::CreateAndAddAutomaticSystem(context, Lambda,
		{
			{1, 2, 1},
			{u8"fun2 1-2-1"}
		});
	auto b3 = Noodles::CreateAndAddAutomaticSystem(context, Lambda,
		{
			{2, 1, 1},
			u8"fun2 2_1_1"
	});

	auto b4 = Noodles::CreateAndAddAutomaticSystem(context, [&](Noodles::ContextWrapper& context, Noodles::AutoComponentQuery<Tuple> query)
	{
		PrintSystemProperty(context);
		auto sys = Noodles::CreateAutomaticSystem(context.GetContext().GetStructLayoutManager(), Lambda);
		context.AddTemporarySystemNodeNextFrame(*sys, { u8"defer_func" });
		context.AddTemporarySystemNode(*sys, {u8"imp func"});

	},
		{
			{2, 1, 3},
			u8"TempTemp 2-1-3"
		});
	auto b5 = Noodles::CreateAndAddAutomaticSystem(context, TestFunction,
		{
			{1,1,3},
			u8"fun4 1-1-3"
		});

	int index2 = 0;

	tcontext.AddGroupThread({}, 10);
	bool re = context.Commited(tcontext, {});
	tcontext.ExecuteContextThreadUntilNoExistTask();
	*/

	return 0;
}