#include "Noodles/NoodlesMemory.h"


namespace Noodles::Memory
{

	constexpr std::size_t MinPageSize = 4 * 1024;
	constexpr std::size_t MemoryDescription = 20 * sizeof(nullptr);

	void HugePage::SubRef() const { 
		if(Ref.SubRef()) {
			this->~HugePage();
			delete[](reinterpret_cast<std::byte const*>(this));
		}
	};

	auto HugePage::CreateInstance(std::size_t MinSize) -> Ptr
	{
		std::size_t RequireSize = MemoryDescription + MinSize + sizeof(HugePage);
		auto Mod = (RequireSize % MinPageSize);
		std::size_t TotalSize = RequireSize / MinPageSize;
		TotalSize += (MinPageSize - Mod);
		std::size_t AllocateSize = TotalSize - MemoryDescription;

		auto Adress = new std::byte[AllocateSize];
		assert((reinterpret_cast<std::size_t>(Adress) % alignof(HugePage)) == 0);
		Ptr Result = new (Adress) HugePage{ std::span<std::byte>{ Adress + sizeof(HugePage), AllocateSize - sizeof(HugePage)}};
		return Result;
	}

	ChunkPage::ChunkPage(HugePage::Ptr Owner, std::size_t MinChunkSize, std::byte* Buffer, std::size_t ChunksCount)
		: Owner(std::move(Owner)), ChunkSize(MinChunkSize), Chunks({reinterpret_cast<ChunkStatus*>(Buffer), ChunksCount})
	{
		new (Chunks.data()) ChunkStatus[ChunksCount];
		Buffer = Buffer + (sizeof(ChunkStatus) * ChunksCount);
		std::size_t Index = 0;
		for (auto& Ite : Chunks)
		{
			Ite.Index = Index++;
			Ite.Adress = Buffer;
			Buffer += MinChunkSize;
		}
	}

	ChunkPage::~ChunkPage()
	{
		for (auto& Ite : Chunks)
		{
			assert(!Ite.BeingUsed);
		}
	}

	auto ChunkPage::CreateInstance(HugePage::Ptr PagePtr, std::span<std::byte> UsedBuffer, std::size_t MinChunkSize) -> Ptr
	{
		assert(PagePtr);
		if (UsedBuffer.size() >= sizeof(ChunkPage) + sizeof(ChunkStatus) + MinChunkSize + sizeof(Chunk))
		{
			std::size_t LastSize = UsedBuffer.size() - sizeof(ChunkPage);
			std::size_t Count = LastSize / (sizeof(ChunkStatus) + MinChunkSize);
			if (Count >= 1)
			{
				Ptr Result = new (UsedBuffer.data()) ChunkPage{ std::move(PagePtr), MinChunkSize, UsedBuffer.subspan(sizeof(ChunkPage)).data(), Count};
				return Result;
			}
		}
		return {};
	}

	auto ChunkPage::CreateInstance(std::size_t MinChunkSize, std::size_t MinPageSize) -> Ptr
	{
		auto Page = HugePage::CreateInstance(MinPageSize);
		if (Page)
		{
			return CreateInstance(Page, MinChunkSize);
		}
		return {};
	}

	void ChunkPage::SubRef() const
	{
		if (Ref.SubRef())
		{
			auto TOwner = std::move(Owner);
			this->~ChunkPage();
		}
	}

	auto ChunkPage::TryAllocate(std::size_t RequireSize) -> Chunk::Ptr
	{
		auto lg = std::lock_guard(Mutex);
		std::size_t ChunkCount = (RequireSize / ChunkSize) + 1;
		if(ChunkCount * ChunkSize < RequireSize + sizeof(Chunk))
			ChunkCount += 1;
		std::optional<std::size_t> Last;
		for (std::size_t I = 0; I < Chunks.size(); ++I)
		{
			auto& Ref = Chunks[I];
			if (Ref.BeingUsed)
			{
				Last.reset();
			}
			else {
				if(!Last.has_value())
					Last = I;
				if (I - *Last + 1 >= ChunkCount)
				{
					for (std::size_t I2 = *Last; I2 <= I; ++I2)
					{
						auto& Ref = Chunks[I2];
						assert(!Ref.BeingUsed && Ref.UsedChunkStatusCount == 0);
						Ref.StartupChunkStatusIndex = *Last;
						Ref.UsedChunkStatusCount = ChunkCount;
						Ref.BeingUsed = true;
					}
					return new Chunk{
						this,
						*Last,
						{Chunks[*Last].Adress + sizeof(Chunk), ChunkSize * ChunkCount - sizeof(Chunk)}
					};
				}
			}
		}
		return {};
	}

	void ChunkPage::Chunk::SubRef()
	{
		if (Ref.SubRef())
		{
			auto TOwner = std::move(Owner);
			assert(TOwner);
			TOwner->Release(ChunkIndex);
			this->~Chunk();
		}
	}

	void ChunkPage::Release(std::size_t ChunkIndex)
	{
		auto lg = std::lock_guard(Mutex);
		auto& Ref = Chunks[ChunkIndex];
		assert(ChunkIndex < Chunks.size());
		assert(Ref.BeingUsed && Ref.UsedChunkStatusCount != 0);
		std::size_t Count = Ref.UsedChunkStatusCount;
		for (auto I = 0; I < Count; ++I)
		{
			auto& Ref = Chunks[I + ChunkIndex];
			Ref.BeingUsed = false;
			Ref.UsedChunkStatusCount = 0;
			Ref.StartupChunkStatusIndex = 0;
		}
	}
}
