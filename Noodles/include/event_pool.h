#pragma once
#include "interface.h"
#include "memory.h"
#include <map>
#include <shared_mutex>
namespace Noodles::Implement
{

	struct SimilerEventPool : EventPoolWriteWrapperInterface
	{

		SimilerEventPool(MemoryPageAllocator& allocator, const TypeLayout& layout) noexcept;
		~SimilerEventPool();

		virtual void construct_event(void(*construct)(void*, void*), void* para, void(*deconstruct)(void*)noexcept) override;

		void update();

		EventPoolMemoryDescription* read_lock(size_t mutex_size, void* mutex);
		void write_lock(size_t mutex_size, void* mutex);

		const TypeLayout& layout() const noexcept { return m_layout; }
	private:

		EventPoolMemoryDescription* allocate_new_page();
		EventPoolMemoryDescription* free_page(EventPoolMemoryDescription*) noexcept;

		MemoryPageAllocator& m_allocator;
		TypeLayout m_layout;

		std::mutex write_mutex;
		EventPoolMemoryDescription* write_top = nullptr;
		EventPoolMemoryDescription* last_write_desc = nullptr;
		size_t last_index = 0;

		std::shared_mutex read_mutex;
		EventPoolMemoryDescription* read_top = nullptr;
		size_t last_read_index = 0;

		size_t target_size = 0;
		size_t max_event_count = 0;
	};

	struct EventPool : EventPoolInterface
	{
		virtual EventPoolMemoryDescription* read_lock(const TypeLayout& layout, size_t mutex_size, void* mutex) noexcept override;
		virtual void read_unlock(size_t mutex_size, void* mutex) noexcept override;
		virtual EventPoolWriteWrapperInterface* write_lock(const TypeLayout& layout, size_t mutex_size, void* mutex) noexcept override;
		virtual void write_unlock(EventPoolWriteWrapperInterface*, size_t mutex_size, void* mutex) noexcept override;

		EventPool(MemoryPageAllocator&) noexcept;
		~EventPool() noexcept;
		void clean_all();
		void update();
		SimilerEventPool* find_pool(const TypeLayout& layout);
		MemoryPageAllocator& m_allocator;
		std::shared_mutex m_event_list_mutex;
		std::map<TypeLayout, SimilerEventPool> m_event_list;
	};
}