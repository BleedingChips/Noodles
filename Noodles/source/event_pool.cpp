#include "../include/event_pool.h"
namespace Noodles::Implement
{
	static constexpr size_t min_page_event_count = 16;

	SimilerEventPool::SimilerEventPool(MemoryPageAllocator& allocator, const TypeLayout& layout) noexcept
		: m_allocator(allocator), m_layout(layout)
	{
		size_t aligned_space = (layout.align > sizeof(nullptr) ? layout.align - sizeof(nullptr) : 0);
		size_t target_size = sizeof(EventPoolMemoryDescription) + aligned_space + (sizeof(void (*)(void*) noexcept) + layout.size) * min_page_event_count;
		auto [i,k] = MemoryPageAllocator::pre_calculte_size(target_size);
		target_size = i;
		max_event_count = target_size - sizeof(EventPoolMemoryDescription) - aligned_space;
		max_event_count = max_event_count / (sizeof(void (**)(void*) noexcept) + layout.size);
		assert(max_event_count >= min_page_event_count);
	}

	SimilerEventPool::~SimilerEventPool()
	{
		std::lock_guard lg(write_mutex);
		while (write_top != nullptr)
			write_top = free_page(write_top);
		last_write_desc = nullptr;
		last_index = 0;

		std::unique_lock ul(read_mutex);
		while (read_top != nullptr)
			read_top = free_page(read_top);
		last_read_index = 0;
	}

	EventPoolMemoryDescription* SimilerEventPool::allocate_new_page()
	{
		auto [buffer, page_size] = m_allocator.allocate(target_size);
		EventPoolMemoryDescription* head = new (buffer) EventPoolMemoryDescription{};
		auto shift = reinterpret_cast<std::byte*>(head + 1);
		head->deconstructor_start = reinterpret_cast<void (**)(void*) noexcept>(shift);
		void* next_shift = shift + sizeof(void (*)(void*) noexcept) * max_event_count;
		size_t last_sapce = page_size - sizeof(EventPoolMemoryDescription) - sizeof(void (*)(void*) noexcept) * max_event_count;
		auto re = std::align(m_layout.align, m_layout.size * max_event_count, next_shift, last_sapce);
		assert(re != nullptr);
		head->event_start = reinterpret_cast<std::byte*>(next_shift);
		return head;
	}

	EventPoolMemoryDescription* SimilerEventPool::free_page(EventPoolMemoryDescription* in) noexcept
	{
		assert(in != nullptr);
		EventPoolMemoryDescription* next = in->next;
		for (size_t i = 0; i < in->count; ++i)
		{
			if (in->deconstructor_start[i] != nullptr)
				in->deconstructor_start[i](in->deconstructor_start + m_layout.size * i);
			else
				break;
		}
		in->~EventPoolMemoryDescription();
		MemoryPageAllocator::release(reinterpret_cast<std::byte*>(in));
		return next;
	}

	void SimilerEventPool::update()
	{
		std::lock_guard lg(write_mutex);
		std::unique_lock ul(read_mutex);
		std::swap(read_top, write_top);
		std::swap(last_read_index, last_index);
		while (write_top != nullptr)
			write_top = free_page(write_top);
		last_write_desc = nullptr;
		last_index = 0;
	}

	void SimilerEventPool::construct_event(void(*construct)(void*, void*), void* para, void(*deconstruct)(void*)noexcept)
	{
		if (write_top == nullptr)
		{
			write_top = allocate_new_page();
			last_write_desc = write_top;
			last_index = 0;
		}
		assert(last_write_desc != nullptr);
		if (last_write_desc->count >= max_event_count)
		{
			auto ptr = allocate_new_page();
			last_write_desc->next = ptr;
			ptr->front = last_write_desc;
			last_write_desc = ptr;
		}
		construct(static_cast<void*>(reinterpret_cast<std::byte*>(last_write_desc->event_start) + last_index * layout().size), para);
		last_write_desc->deconstructor_start[last_index] = deconstruct;
		++last_write_desc->count;
	}

	void SimilerEventPool::write_lock(size_t mutex_size, void* mutex)
	{
		assert(mutex_size >= sizeof(std::lock_guard<std::shared_mutex>));
		new (mutex) std::lock_guard<std::mutex>(write_mutex);
	}

	EventPoolMemoryDescription* SimilerEventPool::read_lock(size_t mutex_size, void* mutex)
	{
		assert(mutex_size >= sizeof(std::shared_lock<std::shared_mutex>));
		new (mutex) std::shared_lock<std::shared_mutex>(read_mutex);
		return read_top;
	}

	EventPool::EventPool(MemoryPageAllocator& allocate) noexcept
		: m_allocator(allocate) {}

	EventPool::~EventPool() noexcept
	{
		std::unique_lock ul(m_event_list_mutex);
		m_event_list.clear();
	}

	void EventPool::clean_all()
	{
		std::lock_guard lg(m_event_list_mutex);
		m_event_list.clear();
	}

	EventPoolMemoryDescription* EventPool::read_lock(const TypeLayout& layout,  size_t mutex_size, void* mutex) noexcept
	{
		assert(mutex_size >= sizeof(std::shared_lock<std::shared_mutex>));
		std::shared_lock<std::shared_mutex>* ss = static_cast<std::shared_lock<std::shared_mutex>*>(mutex);
		auto ptr = find_pool(layout);
		return ptr->read_lock(mutex_size, mutex);
	}

	void EventPool::read_unlock(size_t mutex_size, void* mutex) noexcept
	{
		assert(mutex_size >= sizeof(std::shared_lock<std::shared_mutex>));
		std::shared_lock<std::shared_mutex>* ss = static_cast<std::shared_lock<std::shared_mutex>*>(mutex);
		ss->~shared_lock();
	}

	EventPoolWriteWrapperInterface* EventPool::write_lock(const TypeLayout& layout, size_t mutex_size, void* mutex) noexcept
	{
		auto pool = find_pool(layout);
		pool->write_lock(mutex_size, mutex);
		return pool;
	}

	void EventPool::write_unlock(EventPoolWriteWrapperInterface*, size_t mutex_size, void* mutex) noexcept
	{
		assert(mutex_size >= sizeof(std::lock_guard<std::mutex>));
		static_cast<std::lock_guard<std::mutex>*>(mutex)->~lock_guard();
	}

	SimilerEventPool* EventPool::find_pool(const TypeLayout& layout)
	{
		std::shared_lock sl(m_event_list_mutex);
		auto ite = m_event_list.find(layout);
		if (ite == m_event_list.end())
		{
			sl.unlock();
			std::unique_lock ul(m_event_list_mutex);
			ite = m_event_list.emplace(std::piecewise_construct, std::forward_as_tuple(layout), std::forward_as_tuple(m_allocator, layout)).first;
		}
		return &ite->second;
	}

	void EventPool::update()
	{
		std::unique_lock ul(m_event_list_mutex);
		for (auto& ite : m_event_list)
			ite.second.update();
	}

}