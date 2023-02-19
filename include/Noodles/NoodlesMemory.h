#pragma once
#include "Potato/PotatoMisc.h"
#include "Potato/PotatoSmartPtr.h"
#include <mutex>

namespace Noodles::Memory
{

	struct HugePage
	{
		using Ptr = Potato::Misc::IntrusivePtr<HugePage>;

		static auto CreateInstance(std::size_t MinSize) -> Ptr;

		std::span<std::byte> GetBuffer() const { return Buffer; }
		void AddRef() const { Ref.AddRef(); }

		void SubRef() const;

	private:

		HugePage(std::span<std::byte> Buffer) : Buffer(Buffer) {}

		void ReleaseExe() const;

		mutable Potato::Misc::AtomicRefCount Ref;
		std::span<std::byte> const Buffer;

	};


	struct ChunkPage
	{
		using Ptr = Potato::Misc::IntrusivePtr<ChunkPage>;

		struct ChunkStatus
		{
			bool BeingUsed = false;
			std::size_t Index = 0;
			std::size_t StartupChunkStatusIndex = 0;
			std::size_t UsedChunkStatusCount = 0;
			std::byte* Adress = nullptr;
		};

		struct Chunk
		{
			void AddRef() const { Ref.AddRef(); }
			void SubRef();
			using Ptr = Potato::Misc::IntrusivePtr<Chunk>;

			std::span<std::byte> GetBuffer() const { return Buffer; }
			ChunkPage::Ptr GetOwner() const { return Owner; };

		private:

			Chunk(ChunkPage::Ptr Owner, std::size_t ChunkIndex, std::span<std::byte> Buffer)
				: Owner(std::move(Owner)), ChunkIndex(ChunkIndex), Buffer(Buffer) {}

			ChunkPage::Ptr Owner;
			mutable Potato::Misc::AtomicRefCount Ref;

			std::size_t const ChunkIndex;
			std::span<std::byte> const Buffer;
			friend struct ChunkPage;
		};

		static auto CreateInstance(HugePage::Ptr PagePtr, std::size_t MinChunkSize = 128) -> Ptr {
			if (PagePtr)
			{
				auto Span = PagePtr->GetBuffer();
				return CreateInstance(std::move(PagePtr), Span, MinChunkSize);
			}
			return {};
		}

		static auto CreateInstance(HugePage::Ptr PagePtr, std::span<std::byte> UsedBuffer, std::size_t MinChunkSize = 128) -> Ptr;
		static auto CreateInstance(std::size_t MinChunkSize = 128, std::size_t MinPageSize = 1024) -> Ptr;

		void AddRef() const { Ref.AddRef(); }
		void SubRef() const;

		auto TryAllocate(std::size_t RequireSize) -> Chunk::Ptr;

	protected:
		

		ChunkPage(HugePage::Ptr Owner, std::size_t ChunkSize, std::byte* Buffer, std::size_t ChunksCount);
		~ChunkPage();

		HugePage::Ptr Owner;
		mutable Potato::Misc::AtomicRefCount Ref;
		std::mutex Mutex;
		std::size_t const ChunkSize = 0;
		std::span<ChunkStatus> const Chunks;

		void Release(std::size_t ChunkIndex);
	};

	struct ChunkManager
	{
		struct ChunkPageMapping
		{
			ChunkPage::Ptr Front;
			ChunkPage::Ptr Cur;
			ChunkPage::Ptr Next;
		};


	};


}