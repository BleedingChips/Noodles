#pragma once
#include <tuple>
#include <mutex>
#include <array>
namespace Noodles::Implement
{
	
	struct MemoryPageAllocator
	{

		MemoryPageAllocator(size_t storage_page_count = 4) noexcept;
		~MemoryPageAllocator();

		std::tuple<std::byte*, size_t> allocate(size_t target_sapce);
		static void release(std::byte* buffer) noexcept;
		static size_t reserved_size() noexcept;
		static std::tuple<size_t, size_t> pre_calculte_size(size_t) noexcept;
	private:
		struct RawPageHead
		{
			size_t flag = 0x23234345;
			RawPageHead* m_next_page = nullptr;
			~RawPageHead() = default;
		};
		std::mutex m_page_mutex;
		std::array<std::tuple<RawPageHead*, size_t>, 10> m_pages;
		uint64_t m_require_storage;
	};
}