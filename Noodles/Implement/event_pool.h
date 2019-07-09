#pragma once
#include "../Interface/event_interface.h"
#include "memory.h"
#include <map>
#include <shared_mutex>
namespace Noodles::Implement
{

	struct SimilerEventPool : EventPoolWrapperInterface
	{

		SimilerEventPool(MemoryPageAllocator& allocator, const TypeInfo& layout) noexcept;
		~SimilerEventPool();

		virtual void construct_event(void(*construct)(void*, void*), void* para, void(*deconstruct)(void*)noexcept) override;
		virtual EventPoolMemoryDescription* top_block() const noexcept { return read_top; }
		void update();

		const TypeInfo& layout() const noexcept { return m_layout; }

	private:

		EventPoolMemoryDescription* allocate_new_page();
		EventPoolMemoryDescription* free_page(EventPoolMemoryDescription*) noexcept;

		MemoryPageAllocator& m_allocator;
		TypeInfo m_layout;

		std::mutex write_mutex;
		EventPoolMemoryDescription* write_top = nullptr;
		EventPoolMemoryDescription* last_write_desc = nullptr;
		size_t last_index = 0;

		EventPoolMemoryDescription* read_top = nullptr;
		size_t last_read_index = 0;

		size_t target_size = 0;
		size_t max_event_count = 0;
	};

	struct EventPool : EventPoolInterface
	{
		std::mutex& read_mutex() noexcept { return m_read_mutex; }

		virtual EventPoolWrapperInterface* register_event(const TypeInfo& info) noexcept override;
		virtual void unregister_event(const TypeInfo& info) noexcept override;

		EventPool(MemoryPageAllocator&) noexcept;
		~EventPool() noexcept;

		void clean_all();
		void update();

		MemoryPageAllocator& m_allocator;
		std::mutex m_read_mutex;
		std::mutex m_event_list_mutex;
		std::map<TypeInfo, std::tuple<std::unique_ptr<SimilerEventPool>, size_t>> m_event_list;
	};
}