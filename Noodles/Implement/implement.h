#pragma once
#include "component_pool.h"
#include "gobal_component_pool.h"
#include "event_pool.h"
#include "system_pool.h"

namespace Noodles
{
	using duration_ms = std::chrono::milliseconds;

	struct ContextImplement : Context
	{
		void loop();
		virtual void exit() noexcept override;
		void set_minimum_duration(std::chrono::milliseconds ds) noexcept { m_target_duration = ds; }
		void set_thread_reserved(size_t tr) noexcept { m_thread_reserved = tr; }
		ContextImplement() noexcept;
	private:
		virtual void insert_asynchronous_work_imp(Implement::AsynchronousWorkInterface* ptr) override;
		virtual operator Implement::ComponentPoolInterface* () override;
		virtual operator Implement::GobalComponentPoolInterface* () override;
		virtual operator Implement::EventPoolInterface* () override;
		virtual operator Implement::SystemPoolInterface* () override;
		virtual Implement::EntityInterfacePtr create_entity_imp() override;
		virtual float duration_s() const noexcept override;
		static void append_execute_function(ContextImplement*) noexcept;
		bool apply_asynchronous_work();
		std::atomic_bool m_available;
		size_t m_thread_reserved = 0;
		std::chrono::milliseconds m_target_duration;
		std::atomic<std::chrono::milliseconds> m_last_duration;
		Implement::MemoryPageAllocator allocator;
		Implement::ComponentPool component_pool;
		Implement::GobalComponentPool gobal_component_pool;
		Implement::EventPool event_pool;
		Implement::SystemPool system_pool;
		std::mutex m_asynchronous_works_mutex;
		std::deque<intrusive_ptr<Implement::AsynchronousWorkInterface>> m_asynchronous_works;
	};
}
