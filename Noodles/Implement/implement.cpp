#include "implement.h"
#include "platform.h"
namespace Noodles
{

	void ContextImplement::loop()
	{
		{
			std::lock_guard lg(m_exception_mutex);
			for (auto& ite : m_exception_list)
			{
				if (ite)
				{
					auto re = std::move(ite);
					std::rethrow_exception(re);
				}
				m_have_exceptions = true;
			}
			m_exception_list.clear();
		}

		size_t platform_thread_count = platform_info::instance().cpu_count() * 2 + 2;
		size_t reserved = (m_thread_reserved > platform_thread_count) ? 0 : (platform_thread_count - m_thread_reserved);
		std::vector<std::thread> mulity_thread(reserved);
		m_available = true;
		for (auto& ite : mulity_thread)
			ite = std::thread(&append_execute_function, this);
		auto last_tick = std::chrono::system_clock::now();
		auto target_duration = m_target_duration;
		m_last_duration = target_duration;

		try {
			while (m_available)
			{
				bool Component = component_pool.update();
				bool GobalComponent = gobal_component_pool.update();
				event_pool.update();
				{
					std::lock_guard lg(component_pool.read_mutex());
					std::lock_guard lg2(gobal_component_pool.read_mutex());
					std::lock_guard lg3(event_pool.read_mutex());
					if (system_pool.update(Component, GobalComponent, component_pool, gobal_component_pool))
					{
						system_pool.asynchro_temporary_system(this);
						while (system_pool.asynchro_apply_system(this) != Implement::SystemPool::ApplyResult::AllDone)
							std::this_thread::yield();

						if (mulity_thread.empty())
							while (apply_asynchronous_work());

						auto current_tick = std::chrono::system_clock::now();
						auto dura = std::chrono::duration_cast<std::chrono::milliseconds>(current_tick - last_tick);
						if (dura < target_duration)
						{
							std::this_thread::sleep_for(target_duration - dura);
							current_tick = std::chrono::system_clock::now();
						}
						m_last_duration = std::chrono::duration_cast<std::chrono::milliseconds>(current_tick - last_tick);
						last_tick = current_tick;
					}
					else
						m_available = false;
				}
			}
			for (auto& ite : mulity_thread)
				ite.join();
			{
				std::lock_guard lg(m_asynchronous_works_mutex);
				m_asynchronous_works.clear();
			}
			system_pool.clean_all();
			event_pool.clean_all();
			gobal_component_pool.clean_all();
			component_pool.clean_all();
		}
		catch (...)
		{
			m_available = false;
			for (auto& ite : mulity_thread)
				ite.join();
			{
				std::lock_guard lg(m_asynchronous_works_mutex);
				m_asynchronous_works.clear();
			}
			throw;
		}
		
	}

	bool ContextImplement::apply_asynchronous_work()
	{
		Potato::Tool::intrusive_ptr<Implement::AsynchronousWorkInterface> ptr;
		{
			std::lock_guard lg(m_asynchronous_works_mutex);
			if (!m_asynchronous_works.empty())
			{
				ptr = std::move(*m_asynchronous_works.rbegin());
				m_asynchronous_works.pop_front();
			}
		}
		if (ptr)
		{
			if (ptr->apply(*this))
			{
				std::lock_guard lg(m_asynchronous_works_mutex);
				m_asynchronous_works.push_back(ptr);
			}
			return true;
		}
		return false;
	}

	ContextImplement::ContextImplement() noexcept : m_available(false), m_thread_reserved(0), m_target_duration(duration_ms{ 0 }), m_last_duration(duration_ms{ 0 }),
		allocator(20), component_pool(allocator), gobal_component_pool(), event_pool(allocator), system_pool(), m_have_exceptions(false)
	{

	}

	void ContextImplement::exit() noexcept
	{
		m_available = false;
	}

	void ContextImplement::append_execute_function(ContextImplement* con) noexcept
	{
		try {
			while (con->m_available)
			{
				Implement::SystemPool::ApplyResult result = con->system_pool.asynchro_apply_system(con, false);
				if (result != Implement::SystemPool::ApplyResult::Applied)
				{
					con->apply_asynchronous_work();
					std::this_thread::yield();
					std::this_thread::sleep_for(Noodles::duration_ms{ 1 });
				}
			}
		}
		catch (...)
		{
			std::lock_guard lg(con->m_exception_mutex);
			con->m_exception_list.push_back(std::current_exception());
			con->m_available = false;
		}
	}

	ContextImplement::operator Implement::ComponentPoolInterface* () { return &component_pool; }
	ContextImplement::operator Implement::GobalComponentPoolInterface* () { return &gobal_component_pool; }
	ContextImplement::operator Implement::EventPoolInterface* () { return &event_pool; }
	ContextImplement::operator Implement::SystemPoolInterface* () { return &system_pool; }
	Implement::EntityInterfacePtr ContextImplement::create_entity_imp() { return Implement::EntityImp::create_one(); }

	float ContextImplement::duration_s() const noexcept { 
		duration_ms tem = m_last_duration;
		return tem.count() / 1000.0f;
	}

	void ContextImplement::insert_asynchronous_work_imp(Implement::AsynchronousWorkInterface* ptr)
	{
		Potato::Tool::intrusive_ptr<Implement::AsynchronousWorkInterface> p = ptr;
		assert(ptr != nullptr);
		std::lock_guard lg(m_asynchronous_works_mutex);
		m_asynchronous_works.push_back(std::move(ptr));
	}

}