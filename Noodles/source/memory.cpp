#include "../include/memory.h"
#include "../include/component_pool.h"
namespace Noodles::Implement
{

	constexpr size_t memory_page_space = 2048;
	constexpr size_t memory_flag = 0x12345678;

	struct MemoryPageHead
	{
		MemoryPageAllocator* owner;
		size_t index;
		size_t flag;
		~MemoryPageHead() = default;
	};

	MemoryPageAllocator::MemoryPageAllocator(size_t storage_page_count) noexcept
		: m_require_storage(storage_page_count)
	{
		for (auto& ite : m_pages)
			ite = { nullptr, 0 };
	}

	size_t MemoryPageAllocator::reserved_size() noexcept
	{
		return sizeof(MemoryPageHead);
	}

	std::tuple<size_t, size_t> MemoryPageAllocator::pre_calculte_size(size_t target_size) noexcept
	{
		target_size += MemoryPageAllocator::reserved_size();
		size_t index = target_size / memory_page_space;
		if (target_size % memory_page_space == 0)
			index -= 1;
		return { (index + 1) * memory_page_space - MemoryPageAllocator::reserved_size(), index };
	}


	MemoryPageAllocator::~MemoryPageAllocator()
	{
		std::lock_guard lg(m_page_mutex);
		for (auto ite : m_pages)
		{
			auto [head, index] = ite;
			while (head != nullptr)
			{
				auto cur = head;
				head = head->m_next_page;
				cur->~RawPageHead();
				delete[] reinterpret_cast<std::byte*>(cur);
			}
		}
	}

	std::tuple<std::byte*, size_t> MemoryPageAllocator::allocate(size_t target_sapce)
	{
		auto [space, index] = pre_calculte_size(target_sapce);
		std::lock_guard lg(m_page_mutex);
		assert(m_pages.size() >= index + 1);
		auto& [head, count] = m_pages[index];
		std::byte* buffer = nullptr;
		if (count == 0)
			buffer = new std::byte[(index + 1) * memory_page_space];
		else {
			assert(head != nullptr);
			auto next = head;
			head = head->m_next_page;
			next->~RawPageHead();
			buffer = reinterpret_cast<std::byte*>(next);
			--count;
		}
		auto ptr = new (buffer) MemoryPageHead{this, index, memory_flag };
		return { buffer + sizeof(MemoryPageHead), space };
	}

	void MemoryPageAllocator::release(std::byte* input) noexcept
	{
		assert(input != nullptr);
		MemoryPageHead* buffer = reinterpret_cast<MemoryPageHead*>(input) - 1;
		assert(buffer->flag == memory_flag);
		size_t index = buffer->index;
		std::lock_guard lg(buffer->owner->m_page_mutex);
		assert(buffer->owner->m_pages.size() > index);
		auto& [old_head, old_index] = buffer->owner->m_pages[index];
		if (old_index >= buffer->owner->m_require_storage)
		{
			buffer->~MemoryPageHead();
			delete[](buffer);
		}
		else {
			buffer->~MemoryPageHead();
			RawPageHead* head = new (buffer) RawPageHead{};
			head->m_next_page = old_head;
			old_head = head;
			++old_index;
		}
	}
}