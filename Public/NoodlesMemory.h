#pragma once
#include <atomic>
#include <array>
#include "Potato/Public/PotatoMisc.h"
#include "Potato/Public/PotatoIntrusivePointer.h"

namespace Noodles::PageMemory
{

	struct Allocator;

	class Page;
	class SubPage;

	struct PageWrapper
	{
		static void AddRef(Page* Ptr);
		static void SubRef(Page* Ptr);
		static void AddRef(SubPage* Ptr);
		static void SubRef(SubPage* Ptr);
	};

	using PagePtr = Potato::Misc::IntrusivePtr<Page, PageWrapper>;
	using SubPagePtr = Potato::Misc::IntrusivePtr<SubPage, PageWrapper>;

	// 大页
	class Page
	{
		mutable Potato::Misc::AtomicRefCount ReferenceCount;
		std::span<SubPage> Pages; 
		friend class MemoryPageWrapper;
		friend class MemoryAllocator;
		friend class MemorySubPage;

		friend struct PageWrapper;
		friend struct Allocator;
	};

	// 子页，小页属于大页的一部分，一般一个大页由很多个小页组成
	class SubPage
	{
		std::span<std::byte> GetChunkData() { return Chunk; }
	public:
		mutable Potato::Misc::AtomicRefCount ReferenceCount;
		std::atomic_bool IsUsing;
		PagePtr PagePtr;
		std::span<std::byte> Chunk;

		friend struct PageWrapper;
		friend struct Allocator;
	};

	

	// 块状内存分配器
	struct ChunkAllocator
	{
		SubPage Allocator(std::size_t MinChunkSize, std::size_t Aligna);
	private:
	};


	
}