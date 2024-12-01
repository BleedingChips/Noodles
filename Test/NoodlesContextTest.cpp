import std;
import PotatoTaskSystem;
import NoodlesComponent;
import NoodlesContext;
import PotatoEncode;

std::mutex PrintMutex;

void PrintSystemProperty(Noodles::ContextWrapper& wrapper)
{
	auto wstr = *Potato::Encode::StrEncoder<char8_t, wchar_t>::EncodeToString(wrapper.GetProperty().display_name);
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
	using Context::Context;
protected:
	void AddContextRef() const override {}
	void SubContextRef() const override {}
};


struct TestSystem : public Noodles::SystemNode
{
	void AddSystemNodeRef() const override {}
	void SubSystemNodeRef() const override {}
	void SystemNodeExecute(Noodles::ContextWrapper& context) override { PrintSystemProperty(context); }
	virtual Noodles::SystemName GetDisplayName() const override { return {}; }
};

void TestFunction(Noodles::ContextWrapper& context, Noodles::AtomicComponentFilter<Tuple2>& fup, Noodles::AtomicSingletonFilter<Tuple2>& filter)
{
	auto P = filter.Get(context);
	PrintSystemProperty(context);
}


int main()
{
	//static_assert(std::is_function_v<decltype([](){})>);

	Noodles::Context::Config fig;
	fig.min_frame_time = std::chrono::seconds{ 1 };

	TestContext context{fig};

	Potato::Task::TaskContext tcontext;
	TestSystem systm;

	auto ent = context.CreateEntity();
	context.AddEntityComponent(ent, Tuple{10086});

	for(std::size_t o = 0; o < 100; ++o)
	{
		auto ent2 = context.CreateEntity();
		context.AddEntityComponent(ent2, Tuple{o});
	}

	auto Lambda = [](Noodles::ContextWrapper& context, Noodles::AtomicComponentFilter<Tuple2> filter)
	{
		PrintSystemProperty(context);
	};

	auto Ker = context.AddSingleton(Tuple2{ std::u8string{u8"Fff"} });


	auto b1 = context.CreateAndAddTickedAutomaticSystem(Lambda,
		{ u8"S133", u8"G11" },
	{
		{1, 1, 1},
	});
	auto b2 = context.CreateAndAddTickedAutomaticSystem(Lambda,
		{ u8"S233", u8"G11" },
	{
		{1, 2, 1},
	});
	auto b3 = context.CreateAndAddTickedAutomaticSystem(Lambda,
		{ u8"S144", u8"G22" },
	{
		{2, 1, 1},
	});

	auto b4 = context.CreateAndAddTickedAutomaticSystem([&](Noodles::ContextWrapper& context, Noodles::AtomicComponentFilter<Tuple> filter)
	{
		PrintSystemProperty(context);
		auto sys = context.CreateAutomaticSystem(Lambda, {u8"Temp", u8"Temp"});
		context.AddTemporaryNodeDefer(*sys, {});

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
		{ u8"TempTemp", u8"G22" },
	{
		{2, 1, 3},
	});
	auto b5 = context.CreateAndAddTickedAutomaticSystem(TestFunction,
		{u8"S4", u8"G11"},
		{1, 1, 3}
		);

	int index2 = 0;

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
					},
					{u8"temp"}
				);
				wrapper.AddTemporaryNodeDefer(*P, {});
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

	tcontext.AddGroupThread({}, 2);
	bool re = context.Commited(tcontext, {});
	tcontext.ProcessTaskUntillNoExitsTask({});

	return 0;
}