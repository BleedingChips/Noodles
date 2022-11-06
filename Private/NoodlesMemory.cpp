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

		Ptr P = new (Data) Page{};

		P->AvailableData = {Data + sizeof(Page), RequireSize - sizeof(Page)};

		return P;
	}

	void ChunkManager::Wrapper::AddRef(ChunkManager const* Ptr)
	{
		assert(Ptr != nullptr);
		Ptr->Count.AddRef();
	}

	void ChunkManager::Wrapper::SubRef(ChunkManager const* Ptr)
	{
		assert(Ptr != nullptr);
		if (Ptr->Count.SubRef())
		{
			Ptr->~ChunkManager();
		}
	}

	auto ChunkManager::Create(std::size_t MinSize)->Ptr
	{
		
	}
}
