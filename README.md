# Noodles
ECS (Entity-Component-System) 框架，基于 cpp20-module 。

> 如面条般顺滑（？）。

目前仍在开发中。。。

## 要求

xmake （可选）

vs2022 17.8.2

>由于各大编译器对于 cpp20-moudle 和 module-std 的支持仍然是残废，所以目前只测试了vs系列的该版本。其他版本能否通过编译不能保证。

>另这里推荐 xmake + vs + resharper c++ 的组合，能够在大部分情况下保证代码的编译，以及语法着色和自动补全。

## 安装

从 Github 上将以下库Clone到本地，并使其保持在同一个父路径下：

```
https://github.com/BleedingChips/Noodles.git
https://github.com/BleedingChips/Potato.git
```

在包含该项目的 xmake.lua 上，添加如下代码即可：
```
includes("../Noodles/")

add_deps("Noodles")
```

Noodles 将以静态链接库的形式被引用到项目中。


运行 `Noodles/xmake_install.ps1` 安装 `xmake`，运行`Noodles/xmake_generate_vs_project.ps1`将在`Noodles/vsxmake2022`下产生Noodles的vs的项目文件，以方便对Noodles进行单独的定制化。

## 后续需要添加的Future

1. TemplateSystem 可在运行中添加进当前的System依赖图中，执行一次后销毁，用以支持临时功能。
2. OutdoorSystem 可在外部随时添加的一次System，只会执行一次，用以提供向外部提供数据的的功能。
3. GobalComponent 全局唯一的Component类型。
4. EventChannel 消息信道，用以提供消息传递功能，没想好怎么设计好。
5. Component的内存页自动扩容以及用户自定义扩容机制。
6. 支持成员函数System。
7. TypeID 的实现。目前该实现依赖于std::type_index，但同命名空间下的同名类的 type_index 是相等的，有冲突风险且没有办法检测，等静态反射进标准后再看看。

## 使用

1. 基本循环

	```C++
	import Noodles;

	int main()
	{
		// 创建一个异步任务系统
		auto task_context = Potato::Task::TaskContext::Create();

		// 设置Noodles的参数
		Noodles::ContextConfig config{
			// 异步任务中的优先级，为默认优先级
			*Potato::Task::TaskPriority::Normal,
			
			// 当次循环最少时间
			std::chrono::seconds{10}
		};

		// 创建一个 Nooldes 系统
		auto context = Context::Create(
			config, task_context, u8"My Funcking Noodle Framework"
		);

		{
			// 创建System，Entity相关操作
		}

		// Noodles 系统向异步任务系统循环提交请求
		// 只有当前 System 不为空才会提交请求，否者直接返回 false
		// Noodles 会不断地往任务系统中提交，直到手动退出或 System 为空。
		context->StartLoop();

		// 异步任务系统开始拉起推荐数量的线程（CPU核数-1）
		// 不调用这个将使用当前线程来跑任务
		task_context->FireThreads();
		
		// 异步任务系统执行任务并阻塞，直到任务列表为空
		task_context->FlushTask();
	}
	```

	Noodles 依赖于 Potato 库内实现的一个异步任务框架 Task，该框架内的任务支持自提交，既当外部提交了一个Task之后，Task可以根据需求多次提交自己。

1. 创建 Entity

	Noodles 为拥有相同 Component 类型组合的 Entity 分配一个独立的 Archetype，Archetype 的生命周期将与创建它的 Context 保持一致，既即使 Archetype 的 Entity 被全部销毁，Archetype 依旧会保留一个实体。

	另外，Noodles 支持一个 Entity 拥有多个相同类型的 Component，但这种 Entity 之间将不被视为同一个Archetype。

	```C++
	// 定义一个 Entity 构造器
	EntityConstructor ec{};

	// 以移动构造创建一个类型为A的Component
	ec.MoveConstruct(A{});

	// 以移动构造创建一个类型为A的Component
	ec.MoveConstruct(A{});

	// 以移动构造创建一个类型为B的Component
	ec.MoveConstruct(B{});

	// 以 Entity 构造器为范本创建一个 Entity
	auto en = context->CreateEntityDefer(ec);
	```

	Entity 可以在外部创建，也可以在循环的过程中创建。但除了 EntityFilter ，当前帧创建的 Entity 只有在下一帧中才会被访问。

	另外的，也可以通过给对应 Component 添加一个类内的类型定义，以标识该 Component 应该是唯一的，如：

	```C++
	struct B
	{
		using NoodlesSingletonRequire = void;
	};
	```

	另，所有的 Component 均需要提供 移动构造函数，默认构造函数，以及常量引用构造函数与析构函数。

1. 创建 System

	Noodles 中的 System 种类有很多，最常用的是 TickSystem，既一种常驻的，每帧都会被调用的 System。

	1. TickSystem

		一般的，一个 TickSystem 由如下代码进行创建：

		```c++
		auto i1 = context->RegisterTickSystemDefer(
			// 当前 TickSystem 所属于的 Layout，该值表示该 System 必须最先调用
			std::numeric_limits<std::int32_t>::min(), 

			// System 的优先级，当两个相同 Layout 的 System 存在读写冲突时，由该值来判定先后顺序
			SystemPriority{}, 

			// 该 System 的名字，Context 内唯一，用以比较当前 System 。
			SystemProperty{ u8"start" },

			// 创建 Filter，用以提供当前 System 对特定 Component 类型的访问途径。
			[](FilterGenerator& Generator) -> UserDefineClassT
			{
				std::vector<SystemRWInfo> infos = {
					SystemRWInfo::Create<A const>(),
					SystemRWInfo::Create<B>(),
				};

				// 该 Filter 对 A 读，对 B 写
				auto filter = Generator.CreateComponentFilter(infos);

				std::vector<SystemRWInfo> infos2 = {
					SystemRWInfo::Create<C>(),
				};
				
				// 该 Filter 对 C 写
				auto filter2 = Generator.CreateComponentFilter(infos2);

				
				return {filter, filter2};
			},

			// TickSystem 本体，其参数类型必须为 SystemContext 和 上面 Lambda 函数的返回值。
			[](SystemContext& context, UserDefineClassT& user_define_class)
			{
				// Balabala
			}
		);
		```

		又或者，牺牲一点编译时间，使用自动版本：

		```C++
		auto i1 = context->RegisterTickSystemAutoDefer(
			std::numeric_limits<std::int32_t>::min(), 
			SystemPriority{}, 
			SystemProperty{ u8"start" },
			[&](SystemContext& context, ComponentFilter<A const, B> fil, ComponentFiletr<C> fil2)
			{
				// Balabala
			}
		);
		```

		该版本将使用 TMP 技术自动产生对应的生成 Filter 的代码。其中，Filter 是 System 访问 Component 的唯一途径。

		所有新创建的 TickSystem 只有在下一帧才会被调用。 

		Noodles 将 TickSystem 分为不同的 Layout ，越小的 Layout 会被先调用，并且只有在更小的 Layout 中的所有 TickSystem 被调用完成后，当前 Layout 才会被调用，既不同 Layout 的 TickSystem 将天然地拥有互斥性。

		相同 Layout 的 TickSystem 会根据其所创建的 Filter 来获取对不同类型的Component的读写属性，用以构建 DependencyGraphic，并以此为依据自动进行异步分发。

		若存在以下 TickSystem ：

		```c++
		// 读 A 写 B
		void S1(ComponentFilter<A const, B>);

		// 写 A 写 C
		void S2(ComponentFilter<A, C>);

		// 写 A 写 D
		void S3(ComponentFilter<A const, D>);
		```

		由于 S2 需要写 A，而 S1，S3 需要读 A，所以 S2 与 S1，S2 与 S3 相互冲突，而 S1 与 S3 之间可以并行。为了确定他们之间的顺序，则通过各自的 SystemPriority 的值来进行判断。

		```C++
		struct SystemPriority
		{
			// 第一优先级
			std::int32_t primary_priority = 0;

			// 第二优先级
			std::int32_t second_priority = 0;

			// 特例化优先级
			std::partial_ordering (*compare)(SystemProperty const& self, SystemProperty const& target) = nullptr;
		}
		```

		优先级含有三层优先级，从上到下依次计算，越小的值默认优先级越高，越先调用。在上述例子中，需要分别对 S1 与 S2，S2 与 S3 分别进行优先级计算。计算的结果有四种，分别是 优先于，延后于，互斥，未定义。其中，互斥将会保证这两个 TickSystem 不会同时调用，但不会保证先后顺序。
		
		若结果为未定义（既 primary_priority，second_priority 均相等，且 compare 函数双方均返回 std::partial_ordering::unordered），此时将会取消创建 TickSystem，直接返回。

	1. TickSystem 并行化

		TickSystem 能够向 Context 提交申请以并行化执行特定次数自身：

		```C++
		void S1(SystemContext& sys_context)
		{
			// 当前 TickSystem 是正常调用状态
			if (sys_context.GetSystemCategory() == SystemCategory::Normal)
			{
				// 请求调用自身10次，不包含本次，在自身函数返回后开始调用
				sys_context->StartSelfParallel(sys_context, 10);


				// 返回自身开始并行化
				return;
			}

			// 上述调用的10次均被执行完毕后的状态。
			else if (sys_context.GetSystemCategory() == SystemCategory::FinalParallel)
			{
				
			}
			
			// 正在并行调用时的状态，函数内的数据冲突需要自己维护
			else if(sys_context.GetSystemCategory() == SystemCategory::Parallel)
			{
				// 为 0 - 9 的数字，只会出现一次。
				auto index = sys_context.GetParallelIndex();

				// 该状态无法再次请求并行
			}
		}
		```

		TickSystem 的执行只有到并行化完毕后才算执行完毕。

1. 创建 Filter

	Filter 是唯一一种在 System 内访问 Component 数据的途径。一般在 System 的创建的同时创建。

	Filter 有两种，分别为 ComponentFilter，EntityFilter。前者能访问所有含对应类型的 Entity，后者只能访问特定类型的单个 Entity 。

	Filter 的创建如下：

	```C++

	struct UserDefineClassT
	{
		SystemComponentFilter::Ptr filter1;
		SystemEntityFilter::Ptr filter1;
	};

	auto i1 = context->RegisterTickSystemDefer(
			0,
			SystemPriority{}, 
			SystemProperty{ u8"start" },
			// 创建 Filter，用以提供当前 System 对特定 Component 类型的访问途径。
			[](FilterGenerator& Generator) -> UserDefineClassT
			{
				std::vector<SystemRWInfo> infos = {
					SystemRWInfo::Create<A const>(),
					SystemRWInfo::Create<B>(),
				};

				// 创建 Component Filter
				auto filter = Generator.CreateComponentFilter(infos);

				std::vector<SystemRWInfo> infos2 = {
					SystemRWInfo::Create<C>(),
				};
				
				// 创建 Entity Filter
				auto filter2 = Generator.CreateEntityFilter(infos2);

				return {filter, filter2};
			},

			// TickSystem 本体，其参数类型必须为 SystemContext 和 上面 Lambda 函数的返回值。
			[](SystemContext& context, UserDefineClassT& user_define_class)
			{
				// Balabala
			}
		);
	```

1. 退出循环

	```C++
	context->RequireExist();

	void S1(SystemContext& context)
	{
		context->RequireExits();
	}
	```