#include "Noodles/NoodlesMemory.h"


namespace Noodles::Memory
{

	constexpr std::size_t MinPageSize = 4 * 1024;
	constexpr std::size_t MemoryDescription = 20 * sizeof(nullptr);

	void HugePage::AddRef() const { Ref.AddRef(); }
	void HugePage::SubRef() const { SubRef([](){}); }

	void HugePage::ReleaseExe() const {
		this->~HugePage();
		delete [](this);
	}

	auto HugePage::Create(std::size_t MinSize) -> PtrT
	{
		std::size_t RequireSize = MemoryDescription + MinSize + sizeof(HugePage);
		auto Mod = (RequireSize % MinPageSize);
		std::size_t TotalSize = RequireSize / MinPageSize;
		TotalSize += (MinPageSize - Mod);
		std::size_t AllocateSize = TotalSize - MemoryDescription;

		auto Adress = new std::byte[AllocateSize];
		assert((reinterpret_cast<std::size_t>(Adress) % alignof(HugePage)) == 0);
		HugePage* Ptr = new (Adress) HugePage{ std::span<std::byte>{ Adress + sizeof(HugePage), AllocateSize - sizeof(HugePage)}};
		return Ptr;
	}

	void PageManager::SubRef() const
	{
		if (Ref.SubRef())
		{
			
		}
	}

	auto PageManager::CreateInstance(std::size_t MaxCacheSize) -> PtrT
	{
		auto P = HugePage::Create(sizeof(PageManager) + MaxCacheSize * sizeof(HugePage::PtrT));

		auto Buffer = P->GetBuffer();

		PtrT PM = new (Buffer.data()) PageManager {};

		PM->Owner = std::move(P);

		return PM;
	}
}
