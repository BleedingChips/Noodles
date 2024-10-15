# Noodles

> 如面条般顺滑（？）。

基于`PotatoTask`与`PotatoTaskFlow`的`ECS (Entity-Component-System)`框架，其内部的`Component`采用与`Unity`的`Archetype`相似的内存布局。`System`需要提供其对某个`Component`类型与`Singleton`的读写权限与相关优先级，并自动进行多线程分发，其中后者是全局唯一的数据，且与`Component`相互独立。

该项目要求C++23

## 安装

从 Github 上将以下库Clone到本地，并使其保持在同一个路径下：

```
https://github.com/BleedingChips/Noodles.git
https://github.com/BleedingChips/Potato.git
```

在包含该项目的 xmake.lua 上，添加如下代码即可：

```lua
includes("../Potato/")
includes("../Noodles/")

target(xxx)
	...
	add_deps("Noodles")
target_end()
```

运行 `xmake_install.ps1` 安装 `xmake`，运行`xmake_generate_vs_project.ps1`将在vsxmake2022下产生vs的项目文件。

## 原理

## 使用

1. 引入库

	```cpp
	import Noodles;
	using namespace Noodles;
	```

2. 基本循环

	```C++
	// 实现一个自己的派生类
	struct TestContext : public Noodles::Context
	{
		using Context::Context;
	protected:
		void AddContextRef() const override {}
		void SubContextRef() const override {}
	};

	Noodles::Context::Config fig;
	// 每次循环的最小时间，以控制最大帧率
	fig.min_frame_time = std::chrono::seconds{ 1 };

	// 创建基类的实例
	TestContext context{fig};

	// 拉起一个任务系统
	Potato::Task::TaskContext tcontext;

	// 创建线程
	tcontext.AddGroupThread({}, 2);

	// 提交任务
	context.CommitTask(Lambda, tp);

	// 堵塞并等待所有任务执行完毕
	context.ProcessTaskUntillNoExitsTask({});
	
	```

3. 创建Entity并添加Component

	```cpp
	// 定义Component
	struct Tuple
	{
		std::size_t i = 0;
	};

	struct Tuple2
	{
		std::size_t i = 0;
	};

	// 创建一个空的Entity
	auto ent = context.CreateEntity();

	// 为Entity添加Component
	context.AddEntityComponent(*ent, Tuple{10086});
	context.AddEntityComponent(*ent, Tuple2{10086});
	```

	在任何时候，往Entity里边添加或删除Component都是安全的，但并不会立即生效，只有等到下一次循环后才会生效。要访问缓存，需要使用特定的访问器。添加或删除Component会导致其内存的位置发生变动。

4. 创建`System`并注册

	System的创建有两种方式：

	1. 通过继承的方式创建

		```cpp
		struct TestSystem : public Noodles::SystemNode
		{
			// 实现引用计数功能
			void AddSystemNodeRef() const override {}
			void SubSystemNodeRef() const override {}

			// 导出的对Component,Singleton,UserModify的读写信息
			void FlushMutexGenerator(Noodles::ReadWriteMutexGenerator& generator) const override
			{
				static std::array<Noodles::RWUniqueTypeID, 1> test = {
					Noodles::RWUniqueTypeID::Create<Tuple2>()
				};
				generator.RegisterComponentMutex(std::span(test));
			}

			// System 执行的逻辑
			void SystemNodeExecute(Noodles::ContextWrapper& context) override {  }

			// 获取 System 的名字
			virtual Noodles::SystemName GetDisplayName() const override { return {}; }
		};
		```

	2. 直接通过类型萃取的方式创建：

		```cpp
		auto system_node = context.CreateAutomaticSystem([](
			// Context 的修饰器
			Noodles::ContextWrapper& context, 

			// Component 的访问器，这里对Tuple2 进行写操作
			Noodles::AtomicComponentFilter<Tuple2> filter,

			// Singleton 的访问器，这里对 Tuple1 进行读操作
			Noodles::AtomicSingletonFilter<Tuple2> filter2
			){
			// 访问逻辑
			}, 
			// 名字，分别为名字，组别
			{u8"S133", u8"G11"}
		);
		```

	若两个System同时写一个或者一读一写相同类型的Component或者Singleton，那么就需要对其进行先后决议。
	
	一般每一个System都需要提供三个优先级：`Layer`，`Primary`，`Secondary`。其中，Layer会将两个没有任何读写冲突的System进行先后决议，只有当Layer相同，并且有读写冲突时，才会依次根据Primary和Secondary来进行先后决议。若三值相等，则需要提供额外的方法对此特例进行决议。

	一般的，System的注册方式有两种：

	```cpp
	// 注册常驻 System
	context.AddTickedSystemNode(*system, {0, 0, 0});

	// 注册临时 System
	context.AddTemporarySystemNodeDefer(*system, 0);
	```

	其中，常驻只会在下一帧中执行，临时只会执行一次，并且优先级高于当前Layer中的所有常驻System。

5. `System`的内部并行

	在一个System执行的时候，可以拉起任意数量的自己，并行执行（并行的最大个数取决于拉起的线程池个数）：

	```cpp
	auto system_node = context.CreateAutomaticSystem([](
		// Context 的修饰器
			Noodles::ContextWrapper& context
		){

			auto info = wrapper.GetParrallelInfo();

			// 如果当前是正常执行
			if(info.status == Noodles::ParallelInfo::Status::None)
			{
				// 将自己重新并行提交
				wrapper.CommitParallelTask(
					// 玩家自定义数据
					0, 
					// 总共需要执行的次数
					10086, 
					
					// 分成四个不同的任务去执行
					4
				);
			}else 
			
			// 如果当前是并行执行中
			if(info.status == Noodles::ParallelInfo::Status::Parallel)
			{
				std::lock_guard lg(PrintMutex);
				std::println("Parallel - {0} {1}", info.current_index, info.user_index);
			}else

			// 如果当前是并行执行中的最后一次执行
			{
				std::lock_guard lg(PrintMutex);
				std::println("Done - {0} {1}", info.current_index, info.user_index);
			}
			
		
		}, 
			// 名字，分别为名字，组别
			{u8"S133", u8"G11"}
	);
	```

6. `Component`的访问：

	对Component数据的访问方式有两种，一种是直接遍历所有的Component，一种是对单个Entity进行访问。

	```cpp

	Entity ent = context.CreateEntity();

	lambda = [=](
		Noodles::ContextWrapper& context,  Noodles::AtomicComponentFilter<Tuple> filter
		){
			// 临时数据，减少内存分配
			decltype(filter)::OutputIndexT output;

			// 内存块计数
			std::size_t ite_index = 0;

			// 循环访问不同的内存块
			while(
				Noodles::Context::ComponentWrapper wrapper = filter.IterateComponent_AssumedLocked(context.GetContext(), ite_index, output)
				)
			{
				++ite_index;

				// 读取数据
				std::span<Tuple> ref = filter.GetByIndex<0>(wrapper);
			}

			// 对单个Entity进行访问
			auto wrap = filter.ReadEntity_AssumedLocked(context.GetContext(), *ent, output);
			if(wrap)
			{
				// 直接获取数据
				Tuple* ref = filter.GetByIndex<0>(wrap);
			}
		};


	auto system_node = context.CreateAutomaticSystem([](
		// Context 的修饰器
			Noodles::ContextWrapper& context
		){

			auto info = wrapper.GetParrallelInfo();

			// 如果当前是正常执行
			if(info.status == Noodles::ParallelInfo::Status::None)
			{
				// 将自己重新并行提交
				wrapper.CommitParallelTask(
					// 玩家自定义数据
					0, 
					// 总共需要执行的次数
					10086, 
					
					// 分成四个不同的任务去执行
					4
				);
			}else 
			
			// 如果当前是并行执行中
			if(info.status == Noodles::ParallelInfo::Status::Parallel)
			{
				std::lock_guard lg(PrintMutex);
				std::println("Parallel - {0} {1}", info.current_index, info.user_index);
			}else

			// 如果当前是并行执行中的最后一次执行
			{
				std::lock_guard lg(PrintMutex);
				std::println("Done - {0} {1}", info.current_index, info.user_index);
			}
			
		
		}, 
			// 名字，分别为名字，组别
			{u8"S133", u8"G11"}
	);
	```
