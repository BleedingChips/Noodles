module;

#include <cassert>

module NoodlesMemory;

namespace Noodles::Memory
{
	void* HugePageMemoryResource::do_allocate(size_t _Bytes, size_t _Align)
	{
		DefaultIntrusiveInterface::AddRef();
		PoolResource.allocate(_Bytes, _Align);
	}

	void HugePageMemoryResource::do_deallocate(void* _Ptr, size_t _Bytes, size_t _Align)
	{
		PoolResource.deallocate(_Ptr, _Bytes, _Align);
		DefaultIntrusiveInterface::SubRef();
	}

	bool HugePageMemoryResource::do_is_equal(const memory_resource& _That) const noexcept
	{
		return this == static_cast<HugePageMemoryResource const*>(&_That);
	}

	void HugePageMemoryResource::Release()
	{
		auto MR = PoolResource.upstream_resource();
		this->~HugePageMemoryResource();
		MR->deallocate(this, sizeof(HugePageMemoryResource), alignof(HugePageMemoryResource));
	}

	HugePageMemoryResource::HugePageMemoryResource(std::pmr::memory_resource* UpStreamResource)
		: PoolResource(UpStreamResource)
	{
		
	}
	auto HugePageMemoryResource::Create(Optional Optional, std::pmr::memory_resource* UpResource) -> Ptr
	{
		if(UpResource != nullptr)
		{
			auto Adress = UpResource->allocate(sizeof(HugePageMemoryResource), alignof(HugePageMemoryResource));
			if(Adress != nullptr)
			{
				return new (Adress) HugePageMemoryResource{ UpResource };
			}
		}
		return {};
	}
	/*
	std::byte* AllocatorT::allocate(std::size_t ByteCount)
	{
		if (Ptr)
		{
			return Ptr->allocate(ByteCount);
		}
		else {
			return std::allocator<std::byte>{}.allocate(ByteCount);
		}
	}

	void AllocatorT::deallocate(std::byte* Adress, std::size_t ByteCount)
	{
		if (Ptr)
		{
			return Ptr->deallocate(Adress, ByteCount);
		}
		else {
			return std::allocator<std::byte>{}.deallocate(Adress, ByteCount);
		}
	}

	constexpr std::size_t MinPageSize = 4 * 1024;
	constexpr std::size_t MemoryDescription = 20 * sizeof(nullptr);

	void HugePageT::SubRef() const {
		if(Ref.SubRef()) {
			auto This = const_cast<HugePageT*>(this);
			auto Allo = std::move(This->Allocator);
			auto Count = This->Buffer.size() + sizeof(HugePageT);
			This->~HugePageT();
			Allo.deallocate(reinterpret_cast<std::byte*>(This), Count);
		}
	};

	auto HugePageT::CreateInstance(std::size_t MinSize, AllocatorT Allocator) -> PtrT
	{
		std::size_t RequireSize = MemoryDescription + MinSize + sizeof(HugePageT);
		auto Mod = (RequireSize % MinPageSize);
		std::size_t TotalSize = RequireSize / MinPageSize;
		TotalSize += (MinPageSize - Mod);
		std::size_t AllocateSize = TotalSize - MemoryDescription;

		auto Adress = Allocator.allocate(AllocateSize);
		assert((reinterpret_cast<std::size_t>(Adress) % alignof(HugePageT)) == 0);
		PtrT Result = new (Adress) HugePageT{std::move(Allocator), std::span<std::byte>{ Adress + sizeof(HugePageT), AllocateSize - sizeof(HugePageT)}};
		return Result;
	}

	ChunkPageT::ChunkPageT(HugePageT::PtrT Owner, std::size_t MinChunkSize, std::byte* Buffer, std::size_t ChunksCount)
		: Owner(std::move(Owner)), ChunkSize(MinChunkSize), Chunks({reinterpret_cast<ChunkT*>(Buffer), ChunksCount})
	{
		new (Chunks.data()) ChunkT[ChunksCount];
		ChunkDataAdress = Buffer + (sizeof(ChunkT) * ChunksCount);
		std::size_t Index = 0;
		for (auto& Ite : Chunks)
		{
			Ite.Index = Index++;
		}
	}

	ChunkPageT::~ChunkPageT()
	{
		for (auto& Ite : Chunks)
		{
			assert(!Ite.BeingUsed);
			Ite.~ChunkT();
		}
	}

	auto ChunkPageT::CreateInstance(HugePageT::PtrT PagePtr, std::span<std::byte> UsedBuffer, std::size_t MinChunkSize) -> PtrT
	{
		assert(PagePtr);
		if (UsedBuffer.size() >= sizeof(ChunkPageT) + MinChunkSize + sizeof(ChunkT))
		{
			std::size_t LastSize = UsedBuffer.size() - sizeof(ChunkPageT);
			std::size_t Count = LastSize / (sizeof(ChunkT) + MinChunkSize);
			if (Count >= 1)
			{
				PtrT Result = new (UsedBuffer.data()) ChunkPageT{ std::move(PagePtr), MinChunkSize, UsedBuffer.subspan(sizeof(ChunkPageT)).data(), Count};
				return Result;
			}
		}
		return {};
	}

	auto ChunkPageT::CreateInstance(std::size_t MinChunkSize, std::size_t MinPageSize, AllocatorT Allocator) -> PtrT
	{
		auto Page = HugePageT::CreateInstance(MinPageSize, Allocator);
		if (Page)
		{
			return CreateInstance(Page, MinChunkSize);
		}
		return {};
	}

	void ChunkPageT::SubRef() const
	{
		if (Ref.SubRef())
		{
			auto TOwner = std::move(Owner);
			this->~ChunkPageT();
		}
	}

	auto ChunkPageT::TryAllocate(std::size_t RequireSize) -> ChunkT::PtrT
	{
		auto lg = std::lock_guard(Mutex);
		std::size_t ChunkCount = (RequireSize / ChunkSize) + 1;
		if(ChunkCount * ChunkSize < RequireSize + sizeof(ChunkT))
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
						Ref.BeingUsed = true;
					}
					auto Top = &(Chunks[*Last]);
					Top->Owner = this;
					Top->UsedChunkStatusCount = ChunkCount;
					Top->Buffer = { ChunkDataAdress + ChunkSize * *Last, ChunkSize * ChunkCount };
					return ChunkT::PtrT{ Top };
				}
			}
		}
		return {};
	}

	void ChunkPageT::ChunkT::Release() const
	{
		auto TOwner = std::move(Owner);
		assert(TOwner);
		TOwner->Release(Index);
	}

	void ChunkPageT::Release(std::size_t ChunkIndex)
	{
		auto lg = std::lock_guard(Mutex);
		auto& Ref = Chunks[ChunkIndex];
		assert(ChunkIndex < Chunks.size());
		assert(Ref.BeingUsed && Ref.UsedChunkStatusCount != 0);
		std::size_t Count = Ref.UsedChunkStatusCount;
		Ref.UsedChunkStatusCount = 0;
		for (auto I = 0; I < Count; ++I)
		{
			auto& Ref = Chunks[I + ChunkIndex];
			assert(Ref.BeingUsed);
			Ref.BeingUsed = false;
		}
	}
	*/
}
