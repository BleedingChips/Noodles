# Noodles
ECS (Entity-Component-System) Framework

> 好吃过不饺子！

Demo code in `Demo/demo.sln (vs2019)`.

#### How to use

1. Include `Noodles/implement/implement.h`.

1. Define your Components and Systems.

	```cpp
    using namespace Noodles;
	struct Component1;
	struct Component2;
	struct GobalComponent;
	struct System1
	{

		//Defines system's priority layout. It could be ignored. 
		//Default Priority is TickPriority::Normal
		TickPriority tick_layout();

		//Defines system's priority. It could be ignored. 
		//Default Priority is TickPriority::Normal
		TickPriority tick_priority();

		//Defines system's priority with specific system. It could be ignored.
		//Use TypeLayout::create<X>() to create typelayout for specific system.
		//Default Order is TickOrder Undefine.
		// conflig_type_count is a size[3] which represent system conflig, gobal component conflig and component conflig
		TickOrder tick_order(const TypeLayout& layout， const TypeInfo* conflig_type, size_t* conflig_type_count);

		//Define operator() function with special Filter.
		void operator()(Filter<const Component1>&, Context&);

	};

	// lambda or normal function are also an acceptable system.
	```

1. Define and set up ecs context

	```cpp
	ContextImplement imp;
	// Setting up minimum duration with each cycle. Default value is 16ms
	imp.set_duration(duration_ms{16});

	// Setting up reserve threads for other uses. Default value is 0;
	imp.set_thread_reserved(2);
	```

1. Create Components and Systems

	```cpp
	Entity entity = imp.create_entity();
	Component1& ref1 = imp.create_component<Component1>(entity, /*contructional parameter*/);
	Component2& ref2 = imp.create_component<Component2>(entity, /*contructional parameter*/);
	GobalComponent& ref2 = imp.create_gobal_component<Component2>(entity, /*contructional parameter*/);
	System1& sys1 = imp.create_system<System1>(/*contructional parameter*/);
	//Callable object
	imp.create_system([](Filter<Component1>& f){}, TickPriority::Normal, TickPriority::Normal);
	```

1. Have Fun!

	```cpp
	try{
		imp.loop();
	}catch(const Error::SystemOrderConflig&)
	{
		//Handle system order conflig
	}catch(const Error::SystemOrderRecursion&)
	{
		//Handle system order recursion
	}
	```

#### For Dynamic Link Library

* in DLL:

	```cpp
	// Include interface
	#include "Noodles/Interface/interface.h"
	// Define Components or Systems
	
	// using namespace, optional
	using namespace Noodles;
	extern "C" {
		void __declspec(dllexport) init(Context*);
	}
	void __declspec(dllexport) init(Context*)
	{
		// Createing Systems and Components
	}
	```

* in EXE

	```cpp
	ContextImplement imp;
	/*
	Do some thing here
	*/
	auto handle = LoadLibrary(...);
	auto init = (void(*)(PO::ECS::Context*))GetProcAddress(handle, "init");
	init(imp);
	imp.loop();
	//You must free library after imp.loop() to make sure that every data has been destructed.
	FreeLibrary(handle);
	```

#### Special Filter

* EntityFilter

	Access components form specific Entity only if it has those components.

	```cpp
	void s1::operator()(EntityFilter<const Component1, Component2>& f)
	{
		Entity entity;
		f(entity, [](const Component1&, Component2&){});
	}
	```
	

* Filter

	Access components form all Entity only if it has those components.

	```cpp
	void s1::operator()(Filter<const Component1, Component2>& f)
	{
		for(auto ite : f)
		{
			auto& [comp1, comp2] = ite;
			Entity entity = ite.entity();
			...
		}
	}
	```

* EventViewer

	Provide event to next frame for other systems to access.

	```cpp
	void s1::operator()(EventViewer<int>& f)
	{
		f.push(1);
		for(auto ite = f.begin(); ite != f.end(); ++ite)
		{
			const int& event = *ite;
		}
	}
	```

* Context

	Access ecs context.

	```cpp
	void s1::operator()(PO::ECS::Context& f)
	{
	    //Jump out of the loop
	    f.exit();

        // insert asynchronous work
        f.insert_asynchronous_work([](Context& c){
            // continue apply
            return true;
        });
	}
	```

* SystemFilter

	Access other system. 

	```cpp
	void s1::operator()(SystemFilter<System2>& f, SystemFilter<const System2>& f2)
	{
		// make sure that systems exist
		if(f && f2)
		{
			System2* sys = f.operator->();
			const System2* sys2 = f2.operator->();
		}
	}
	```

* GobalFilter

	Access gobal component. 

	```cpp
	void s1::operator()(GobalFilter<GobalComponent>& g1, GobalFilter<const GobalComponent2>& g2)
	{
		// make sure that gobal components exist
		if(g1 && g2)
		{
			GobalComponent* gc1 = g1.operator->();
			const GobalComponent2* gc2 = g2.operator->();
		}
	}
	```


#### System Parallelized

Framework uses the type of special filter from system's `operator()` funtion to detect read-write-property of specific data type, and those read-write-property will be used to make a order graphic, which control the order of system calling and whether two system will be parallelized or not. 

|Filter Type|Read-Write-Property|
|:---:|:---:|
|`Filter<A>`or`EntityFilter<A>`|Write To ComponentA|
|`Filter<const A>`or`EntityFilter<const A>`|Read From ComponentA|
|`SystemWrapper<A>`| Write To SystemA|
|`SystemWrapper<const A>`|Read From SystemA|
|`GobalFilter<A>`|Write To GobalComponentA|
|`GobalFilter<const A>`|Read From GobalComponentA|
|`EntityViewer<A>`|None|

Framework uses the following step to decide system order of two specific system.

1. Compares system layout priority.

	System layout priority comes from function `TickPriority s1::tick_layout()`. `TickPriority` has following 5 values which sorted by its priority.
	> **HighHigh** > **High** > **Normal** > **Low** > **LowLow**

	System with higher priority will be called **Before** system with lower priority. In the same time, two systems with difference priority will never be parallelized. 
	
	If is the same, jump to step 2.

2. Calculate system's read-write-property

	If system s1 needs to write to s2, like `void s1::operator(SystemWrapper<s2>)`, means s1 **may** be called **Before** s2, while if system s1 needs to read from s2, like `void s1::operator(const SystemWrapper<s2>)`, means s1 **may** be called **After** s2. 

	If two systems need to write to each other, an exception of `Error::SystemOrderConflig` **may** be threw. 

	In fact, if those situations occurr, framework will jume to step 4 to calculate user-define priority. If undefine, framework will use the reault from this step, else will use the result of user-define priority.

	If thos situations is not occurred, jump to step 3.

3. Calculate read-write-property of same data type.

	Data type is sperated into following 4 independent channels, sorted by calculating order.

	> **System** > **GobalComponent** > **Component** > **Event**

	If s1 needs to write to DataTypeA, and s2 needs to read from DataTypeA, means s1 **may** be called **Before** s2. 
	
	If s1 needs to write to DataTypeA, read from DataTypeB, and s2 needs to write to DataTypeB, read from DataTypeA, or both of s1 and s2 need to write to s3, an exception of `Error::SystemOrderConflig` **may** be threw. 

	Just like step 2, if those situations occurr, framework will jume to step 4 to calculate user-define priority. 
	
	But event channle has little different. If s1 needs to write to EventA and s2 needs to write to EventB, s1 and s2 **will** not be call at the same time.

	If thos situations is not occurred, those two system **may** be parallelized.
	
4. User-define priority

	System priority just like system layout priority but it came from `TickPriority s1::tick_priority()`. If is the same, framework will call following two functions to get the order.
	
	```cpp
	auto t1 = TickOrder s1::tick_order(TypeLayout::create<s2>());
	auto t2 = TickOrder s2::tick_order(TypeLayout::create<s1>());
	```

	If t1 and t2 is `TickOrder::Undefine`, will not overwrite order.
	
	If t1 and t2 is conflig, an exception of `Error::SystemOrderConflig` **will** be threw.

	Else, overwrite order.

After system order is beening decide, an order graphic will be made. But if s1 before s2, s2 before s3 and s3 before s1, an exception of `Error::SystemOrderRecursion` will be threw.

With the help of order graphic, systems can be Parallelized.

#### More flexible mutual condition determination.

Like filter `Filter<A, B, C>` and `Filter<A, B, D>` in different systems. And there is no Entity with ComponentGroup like `<A, B, C, D>`. Those two systems can be parallelized.

> PS: 英语不太好的我实在是尽力了。。。
