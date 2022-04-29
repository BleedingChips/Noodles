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

	// ��ҳ
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

	// ��ҳ��Сҳ���ڴ�ҳ��һ���֣�һ��һ����ҳ�ɺܶ��Сҳ���
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

	

	// ��״�ڴ������
	struct ChunkAllocator
	{
		SubPage Allocator(std::size_t MinChunkSize, std::size_t Aligna);
	private:
	};


	
}