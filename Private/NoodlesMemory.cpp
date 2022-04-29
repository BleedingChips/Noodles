#include "../Public/NoodlesMemory.h"


namespace Noodles::PageMemory
{
	// 暂时先不管内存分配的，以后慢慢调优

	void PageWrapper::AddRef(Page* Ptr)
	{
		assert(Ptr != nullptr);
		Ptr->ReferenceCount.AddRef();
	}

	void PageWrapper::SubRef(Page* Ptr)
	{
		assert(Ptr != nullptr);
		if (Ptr->ReferenceCount.SubRef())
		{
			Ptr->~Page();
			delete Ptr;
		}
	}

	void PageWrapper::AddRef(SubPage* Ptr)
	{
		assert(Ptr != nullptr);
		Ptr->ReferenceCount.AddRef();
	}

	void PageWrapper::SubRef(SubPage* Ptr)
	{
		assert(Ptr != nullptr);
		if (Ptr->ReferenceCount.SubRef())
		{
			Ptr->PagePtr.Reset();
		}
	}

	SubPage ChunkAllocator::Allocator(std::size_t MinChunkSize, std::size_t Aligna)
	{
		std::size_t Size = sizeof(SubPage) + sizeof(Page);
		if(Aligna > alignof(Size))
			Size += Aligna - alignof(Size);
		std::byte* Data = new std::byte[Size];
		auto PagePtr = new (Data) Page;

	}
}
