import std;
import Potato;
import Noodles;

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


int main()
{
	//static_assert(std::is_function_v<decltype([](){})>);

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

		/*
		std::size_t ite_index = 0;
		while(
			auto wrapper = context.GetContext().ReadEntity_AssumedLocked()
			)
		{
			++ite_index;
			std::span<Tuple> ref = filter.GetByIndex<0>(wrapper);
		}

		auto wrap = filter.ReadEntity_AssumedLocked(context.GetContext(), *ent, output);
		if(wrap)
		{
			Tuple* ref = filter.GetByIndex<0>(wrap);
		}
		*/

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

	/*
	auto b6 = context.CreateAndAddTickedAutomaticSystem([&](Noodles::ContextWrapper& wrapper)
	{
		auto info = wrapper.GetParrallelInfo();
		if(info.status == Noodles::ParallelInfo::Status::None)
		{
			
			PrintSystemProperty(wrapper);
			wrapper.CommitParallelTask(0, 3, 3);
			if(index2 % 2 == 0)
			{
				auto P = wrapper.CreateAutomaticSystem(
					[&](){
						std::lock_guard lg(PrintMutex);
						std::println("defer - {0}", index2);
					}
				);
				wrapper.AddTemporaryNodeDefer(P, { u8"temp" });
			}
			index2 += 1;
		}else if(info.status == Noodles::ParallelInfo::Status::Parallel)
		{
			std::lock_guard lg(PrintMutex);
			std::println("Parallel - {0} {1}", info.current_index, info.user_index);
		}else
		{
			std::lock_guard lg(PrintMutex);
			std::println("Done - {0} {1}", info.current_index, info.user_index);
		}
	},
		{u8"S5", u8"G11"},
		{1, 1, 3}
		);

	std::size_t index = 0;
	*/

	tcontext.AddGroupThread({}, 10);
	bool re = context.Commited(tcontext, {});
	tcontext.ExecuteContextThreadUntilNoExistTask();

	return 0;
}