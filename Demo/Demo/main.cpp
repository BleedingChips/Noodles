#include "..//..//Noodles/implement/implement.h"
#include <random>
#include <iostream>
#include <math.h>
#include <Windows.h>

using namespace std;
using namespace Noodles;

struct Location
{
	float x, y;
};

struct Collision
{
	float Range;
};

struct Velocity
{
	float x, y;
};

struct EaterFlag {};

struct ProviderFlag {
	float Data = 0.0;
};

struct FoodFlag {};

std::mutex cout_mutex;

template<typename Type> struct CallRecord
{
	CallRecord()
	{
		std::lock_guard lg(cout_mutex);
		std::cout << "thread id<" << std::this_thread::get_id() << "> : " << typeid(Type).name() << " - start" << std::endl;
	}

	~CallRecord()
	{
		std::lock_guard lg(cout_mutex);
		std::cout << "thread id<" << std::this_thread::get_id() << "> : " << typeid(Type).name() << " - end" << std::endl;
	}
};

struct DieEvent { size_t index; };


struct MoveSystem
{
	void operator()(Filter<Location, Velocity, const Collision>& f, Context& c)
	{
		CallRecord<MoveSystem> record;
		for (auto ite : f)
		{
			auto [lo, ve, col] = ite;
			lo.x += ve.x * c.duration_s();
			lo.y += ve.y * c.duration_s();
			if ((lo.x - col.Range) < -1.0f)
			{
				lo.x = col.Range - 1.0f;
				ve.x = -ve.x;
			}
			else if ((lo.x + col.Range) > 1.0f)
			{
				lo.x = 1.0f - col.Range;
				ve.x = -ve.x;
			}
			if ((lo.y - col.Range) < -1.0f)
			{
				lo.y = col.Range - 1.0f;
				ve.y = -ve.y;
			}
			else if ((lo.y + col.Range) > 1.0f)
			{
				lo.y = 1.0f - col.Range;
				ve.y = -ve.y;
			}

		}
	}
};

struct CollisionSystem
{

	void operator()(
		Filter<const Location, Collision, const EaterFlag>& eater,
		Filter<const Location, const Collision, const FoodFlag>& food,
		Context& con, EventViewer<DieEvent>& EV, SystemFilter<const MoveSystem>& f
		)
	{
		CallRecord<CollisionSystem> record;
		size_t die = 0;
		for (auto& ite : eater)
		{
			auto& [l, c, flag1] = ite;
			for (auto& ite2 : food)
			{
				auto& [l2, c2, flag2] = ite2;
				auto xs = l.x - l2.x;
				auto ys = l.y - l2.y;
				auto rs = c.Range + c2.Range;
				if ((xs * xs + ys * ys) < rs * rs)
				{
					c.Range += c2.Range * 0.05f;
					if (c.Range > 0.3f)
						c.Range = 0.3f;
					con.destory_entity(ite2.entity());
					++die;
				}
			}
		}
		if (die != 0)
			EV.push(die);
		std::lock_guard lg(cout_mutex);
		for (auto& ite : EV)
			std::cout << "CollisionSystem :: Last Frame die : " << ite.index << std::endl;
	}
};

struct CreaterSystem
{
	void operator()(Filter<const Location, const Collision, ProviderFlag>& s, Context& con)
	{
		CallRecord<CreaterSystem> record;
		for (auto& ite : s)
		{
			auto& [lo, co, flag] = ite;
			if (ran(engine) < flag.Data)
			{
				flag.Data = 0.0;
				auto en = con.create_entity();
				con.create_component<Location>(en, lo);
				con.create_component<Collision>(en, m_range(engine));
				con.create_component<FoodFlag>(en);
			}
			else {
				flag.Data += con.duration_s() * 0.1;
			}
		}
	}

	CreaterSystem(std::default_random_engine& seed, std::uniform_real_distribution<float>& rang, std::uniform_real_distribution<float> vel) : engine(seed), ran(0.0, 1.0), m_range(rang), m_vel(vel) {}
private:
	std::default_random_engine& engine;
	std::uniform_real_distribution<float> ran;
	std::uniform_real_distribution<float>& m_range;
	std::uniform_real_distribution<float>& m_vel;
};

struct Temporary2;

struct Temporary1
{
	void operator()(Context& c) {
		CallRecord<Temporary1> record;
		c.create_temporary_system<Temporary2>();
	}
};

struct Temporary2
{
	void operator()(Context& c) {
		CallRecord<Temporary2> record;
		c.create_temporary_system<Temporary1>();
	}
};


int main()
{
	{

		ContextImplement imp;

		imp.set_thread_reserved(2);
		
		//imp.set_minimum_duration(duration_ms{200});
		
		imp.create_system([&]() {
			std::lock_guard lg(cout_mutex);
			std::cout << "loop start --------------" << std::endl;
		}, TickPriority::HighHigh, TickPriority::HighHigh);
		
		imp.create_system([&]() {
			std::lock_guard lg(cout_mutex);
			std::cout << "loop end --------------" << std::endl;
		}, TickPriority::LowLow, TickPriority::LowLow);
		
		std::random_device r_dev;
		std::default_random_engine engine(r_dev());
		std::uniform_real_distribution<float> location(-0.9f, 0.9f);
		std::uniform_real_distribution<float> range(0.01f, 0.02f);
		std::uniform_real_distribution<float> vel(-0.4f, 0.4f);

		imp.create_system<CollisionSystem>();
		imp.create_system<CreaterSystem>(engine, range, vel);
		imp.create_system<MoveSystem>();
		imp.create_temporary_system<Temporary1>();

		for (size_t i = 0; i < 500; ++i)
		{
			auto entity = imp.create_entity();
			auto& re = imp.create_component<Location>(entity, location(engine), location(engine));
			imp.create_component<Collision>(entity, range(engine));
			imp.create_component<FoodFlag>(entity);
		}

		for (size_t i = 0; i < 4; ++i)
		{
			auto entity = imp.create_entity();
			auto& re = imp.create_component<Location>(entity, location(engine), location(engine));
			imp.create_component<Collision>(entity, range(engine));
			imp.create_component<Velocity>(entity, vel(engine), vel(engine));
			imp.create_component<EaterFlag>(entity);
		}

		for (size_t i = 0; i < 4; ++i)
		{
			auto entity = imp.create_entity();
			auto& re = imp.create_component<Location>(entity, location(engine), location(engine));
			imp.create_component<Collision>(entity, range(engine));
			imp.create_component<Velocity>(entity, vel(engine), vel(engine));
			imp.create_component<ProviderFlag>(entity);
		}

		auto handle = LoadLibrary("renderermodule.dll");
		if (handle)
		{
			void (*init)(Context*, std::mutex*) = (void(*)(Context*, std::mutex*))GetProcAddress(handle, "init");
			init(&imp, &cout_mutex);
		}
		
		imp.insert_asynchronous_work([&](Context& con, float input) {
			{
				std::lock_guard lg(cout_mutex);
				std::cout << "thread id<" << std::this_thread::get_id() << "> : " <<
					"asynchronous_work" << std::endl;
			}
			std::this_thread::sleep_for(duration_ms{ 100 });
			return true;
		}, 6.50f);

		try {
			imp.loop();
		}
		catch (Error::SystemOrderConflig& soc)
		{
			std::cout << "conflig : " << soc.si << " " << soc.ti << std::endl;
		}

		if (handle)
		{
			FreeLibrary(handle);
			handle = nullptr;
		}
	}
}