#include "../Public/NoodlesMemory.h"


namespace Noodles::Memory
{
	void Page::Wrapper::AddRef(Page const* Ptr)
	{
		assert(Ptr != nullptr);
		Ptr->Count.AddRef();
	}

	void Page::Wrapper::SubRef(Page const* Ptr)
	{
		assert(Ptr != nullptr);
		if (Ptr->Count.SubRef())
		{
			Ptr->~Page();
			delete[] (reinterpret_cast<std::byte const*>(Ptr));
		}
	}

	constexpr std::size_t MinPageSize = 4 * 1024;
	constexpr std::size_t MemoryDescription = 20 * sizeof(nullptr);

	auto Page::Create(std::size_t MinSize)->Ptr
	{
		std::size_t RequireSize = MemoryDescription + MinSize + sizeof(Page);
		auto Mod = (RequireSize % MinPageSize);
		if (Mod == 0)
		{
			RequireSize = RequireSize;
		}
		else {
			RequireSize = RequireSize + (MinPageSize - Mod);
		}

		std::byte* Data = new std::byte[RequireSize];

		assert(reinterpret_cast<uint64_t>(Data) % sizeof(nullptr_t) == 0);

		Ptr P = new (Data) Page{};

		P->AvailableData = {Data + sizeof(Page), RequireSize - sizeof(Page)};

		return P;
	}

	void ChunkPage::Wrapper::AddRef(ChunkPage const* Ptr)
	{
		assert(Ptr != nullptr);
		Ptr->Ref.AddRef();
	}

	void ChunkPage::Wrapper::SubRef(ChunkPage const* Ptr)
	{
		assert(Ptr != nullptr);
		if (Ptr->Ref.SubRef())
		{
			for (auto & Ite : Ptr->OwnedPtr)
			{
				Ite.~IntrusivePtr();
			}
			Ptr->~ChunkPage();
		}
	}

	void ChunkPage::Chunk::OwnerWrapper::AddRef(Chunk const* Ptr)
	{
		assert(Ptr != nullptr);
		Ptr->OwnerRef.AddRef();
	}

	void ChunkPage::Chunk::OwnerWrapper::SubRef(Chunk const* Ptr)
	{
		assert(Ptr != nullptr);
		if (Ptr->OwnerRef.SubRef())
		{
			Ptr->~Chunk();
		}
	}

	void ChunkPage::Chunk::ViewWrapper::AddRef(Chunk const* Ptr)
	{
		assert(Ptr != nullptr);
		Ptr->ViewRef.AddRef();
		OwnerWrapper::AddRef(Ptr);
	}

	void ChunkPage::Chunk::ViewWrapper::SubRef(Chunk const* Ptr)
	{
		assert(Ptr != nullptr);
		Ptr->ViewRef.SubRef();
		OwnerWrapper::SubRef(Ptr);
	}

	constexpr std::size_t ChunkManagerElementMinCount = 10;

	auto ChunkAllocator::Create(std::size_t MinChunkElementCount)->Ptr
	{
		auto P = Page::Create(sizeof(ChunkAllocator) + sizeof(Element) * std::max(ChunkManagerElementMinCount, MinChunkElementCount));
		if (P)
		{
			assert((reinterpret_cast<uint64_t>(P->Datas().data()) % alignof(ChunkAllocator)) == 0);

			Ptr Result = new (P->Datas().data()) ChunkAllocator{};

			auto Data = Result.Data() + 1;

			assert((reinterpret_cast<uint64_t>(Data) % alignof(Element)) == 0);

			std::size_t ElementCount = P->Datas().size() - sizeof(ChunkAllocator);

			ElementCount = ElementCount / sizeof(Element);

			assert(ElementCount >= ChunkManagerElementMinCount);

			auto ElementData = new (Data) Element [ElementCount];

			Result->Elements = { ElementData, ElementCount };

			Result->PagePtr = std::move(P);

			return Result;
		}
		return {};
	}

	ChunkPage::Chunk::ViewPtr ChunkAllocator::CreateChunk(std::size_t MinChunkSize)
	{
		std::scoped_lock L1{ Mutex };
	}

	Chunk::Ptr ChunkManager::GetChunk(std::size_t MinSize)
	{
		{
			std::scoped_lock L1{ Mutex };

			for (auto& Ite : Elements)
			{
				if (Ite.ChunkPtr && *Ite.ChunkPtr && Ite.ChunkPtr->Datas.size() == MinSize)
				{
					
				}
			}

		}

		return {};
	}
}
