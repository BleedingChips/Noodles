import std;
import Potato;
import Noodles;

struct A { std::size_t i = 0; };

struct B { using NoodlesThreadSafeType = void;  std::size_t i[2] = { 1, 2 }; };

struct C { std::size_t i[2] = { 1, 2 }; };

struct SysNode : public Noodles::SystemNode
{
	virtual void SystemNodeExecute(Noodles::Context& context) {

		std::array<Noodles::ComponentQuery::OPtr, 10> out_comp;
		std::array<Noodles::SingletonQuery::OPtr, 10> out_sing;

		auto [s, p] = context.GetQueryFromExecuteSystem(out_comp, out_sing);
		
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

int main()
{
	auto sys = Noodles::CreateAutoSystemNode(
		[](
			Noodles::Context& context, 
			Noodles::AutoComponentQuery<A, B>::template Refuse<C> query,
			Noodles::AutoSingletonQuery<A> s_query
		) 
		{
			/*
			Noodles::AutoComponentQueryIterator iterator;
			auto component_query_data = query.GetQueryData();
			auto singleton_query_data = s_query.GetQueryData();

			while (query.QuerySpanAndMoveToNext(context, iterator, component_query_data))
			{
				auto p = component_query_data.Get<0>();
				auto p2 = component_query_data.Get<1>();
				volatile int i = 0;
			}

			if (s_query.Query(context, singleton_query_data))
			{
				auto p = singleton_query_data.Get<0>();
				volatile int i = 0;
			}
			*/
		}
	);


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
		//instance->LoadSystemNode(Noodles::SystemCategory::OnceNextFrame, s1, sys_par);
		sys_par.name = L"TestSystem4!!";
		//instance->LoadSystemNode(Noodles::SystemCategory::OnceNextFrame, s1, sys_par);

		sys_par.name = L"TestSystem66!!";
		instance->LoadSystemNode(Noodles::SystemCategory::Tick, sys_index, sys_par);
		Noodles::Instance::Parameter par;
		par.duration_time = std::chrono::milliseconds{ 3000 };
		auto enti = instance->CreateEntity();
		instance->AddEntityComponent(*enti, A{ 10086 });
		instance->AddEntityComponent(*enti, B{ 100862 });
		instance->Commit(context, par);
		context.CreateThreads(2);
		context.ExecuteContextThreadUntilNoExistTask();
	}

	return 0;
}