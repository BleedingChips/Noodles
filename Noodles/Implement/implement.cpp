#include "../include/implement.h"
#include "../include/platform.h"
namespace Noodles
{

	void ContextImplement::loop()
	{
		size_t platform_thread_count = platform_info::instance().cpu_count() * 2 + 2;
		size_t reserved = (m_thread_reserved > platform_thread_count) ? 0 : (platform_thread_count - m_thread_reserved);
		std::vector<std::thread> mulity_thread(reserved);
		m_available = true;
		for (auto& ite : mulity_thread)
			ite = std::thread(&append_execute_function, this);
		auto last_tick = std::chrono::system_clock::now();
		auto target_duration = m_target_duration;
		m_last_duration = target_duration;

		Tool::scope_guard sg{
			[&, this]() noexcept {
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
		};

		try {
			while (m_available)
			{
				component_pool.update();
				gobal_component_pool.update();
				event_pool.update();
				if (system_pool.update())
				{
					while (system_pool.asynchro_apply_system(this) != Implement::SystemPool::ApplyResult::AllDone)
						std::this_thread::yield();
					system_pool.synchro_apply_template_system(this);

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
		catch (...)
		{
			m_available = false;
			throw;
		}
		
	}

	bool ContextImplement::apply_asynchronous_work()
	{
		Tool::intrusive_ptr<Implement::AsynchronousWorkInterface> ptr;
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

	ContextImplement::ContextImplement() noexcept : m_available(false), m_thread_reserved(0), m_target_duration(16), m_last_duration(duration_ms{ 0 }),
		allocator(20), component_pool(allocator), gobal_component_pool(), event_pool(allocator), system_pool()
	{

	}

	void ContextImplement::exit() noexcept
	{
		m_available = false;
	}

	void ContextImplement::append_execute_function(ContextImplement* con) noexcept
	{
		while (con->m_available)
		{
			while (true)
			{
				Implement::SystemPool::ApplyResult result = con->system_pool.asynchro_apply_system(con, false);
				if (result != Implement::SystemPool::ApplyResult::Applied)
				{
					con->apply_asynchronous_work();
					std::this_thread::yield();
					break;
				}
			}
			std::this_thread::sleep_for(Noodles::duration_ms{1});
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
		Tool::intrusive_ptr<Implement::AsynchronousWorkInterface> p = ptr;
		assert(ptr != nullptr);
		std::lock_guard lg(m_asynchronous_works_mutex);
		m_asynchronous_works.push_back(std::move(ptr));
	}

}