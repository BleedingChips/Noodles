#pragma once
#include "interface.h"
#include "implement/component_pool.h"
#include "implement/gobal_component_pool.h"
#include "implement/event_pool.h"
#include "implement/system_pool.h"

namespace Noodles
{
	using duration_ms = std::chrono::milliseconds;

	namespace Exception {
		struct MultiExceptions : std::exception
		{
			virtual const char* what() const noexcept override;
			MultiExceptions(std::vector<std::exception_ptr> list) : errors(std::move(list)) {}
			std::vector<std::exception_ptr> errors;
		};
	}

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
		std::mutex m_exception_mutex;
		std::vector<std::exception_ptr> m_exception_list;
	};
}
