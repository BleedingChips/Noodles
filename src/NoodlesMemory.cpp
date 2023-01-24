#include "Noodles/NoodlesMemory.h"


namespace Noodles::Memory
{

	constexpr std::size_t MinPageSize = 4 * 1024;
	constexpr std::size_t MemoryDescription = 20 * sizeof(nullptr);

	void PageManager::Page::SubRef() const
	{
		if (Ref.SubRef())
		{
			this->~Page();
		}
	}

	PageManager::Page::~Page()
	{
		auto Temp = std::move(Owner);
	}

	auto PageManager::Allocate(std::size_t MinSize)->PPtrT
	{
		std::size_t RequireSize = MemoryDescription + MinSize + sizeof(Page);
		auto Mod = (RequireSize % MinPageSize);
		std::size_t TotalSize = RequireSize / MinPageSize;
		TotalSize += (MinPageSize - Mod);

		auto Adress = new std::byte[TotalSize - MemoryDescription];
		assert((reinterpret_cast<std::size_t>(Adress) % alignof(Page)) == 0);
		Page* Ptr = new (Adress) Page{ PtrT{this} };

		Ptr->Buffer = {Adress + sizeof(Page), TotalSize  - MemoryDescription  - sizeof(Page)};
		return Ptr;
	}

	void PageManager::Release(Page const* Page)
	{
		if (Page != nullptr)
		{
			Page->~Page();
			delete[](Page);
		}
	}
}
