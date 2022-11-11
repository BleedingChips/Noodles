#pragma once
#include "Potato/Public/PotatoMisc.h"
#include "Potato/Public/PotatoIntrusivePointer.h"
#include <mutex>

namespace Noodles::Memory
{
	struct Page
	{
		struct Wrapper
		{
			void AddRef(Page const* Ptr);
			void SubRef(Page const* Ptr);
		};

		using Ptr = Potato::Misc::IntrusivePtr<Page, Wrapper>;
		static Ptr Create(std::size_t MinSize);
		std::span<std::byte> Datas() const { return AvailableData; }
	private:
		Page() = default;
		mutable Potato::Misc::AtomicRefCount Count;
		std::span<std::byte> AvailableData;
	};

	struct ChunkPage
	{

		struct Wrapper
		{
			void AddRef(ChunkPage const* Ptr);
			void SubRef(ChunkPage const* Ptr);
		};

		using Ptr = Potato::Misc::IntrusivePtr<ChunkPage, Wrapper>;

		struct Chunk
		{
			struct OwnerWrapper
			{
				void AddRef(Chunk const* Ptr);
				void SubRef(Chunk const* Ptr);
			};

			using OwnerPtr = Potato::Misc::IntrusivePtr<Chunk, OwnerWrapper>;

			struct ViewWrapper : private OwnerWrapper
			{
				void AddRef(Chunk const* Ptr);
				void SubRef(Chunk const* Ptr);

				ViewWrapper() = default;
				ViewWrapper(ViewWrapper const&) = default;
				ViewWrapper(OwnerWrapper const&, Chunk const* Ptr) {}
			};

			using ViewPtr = Potato::Misc::IntrusivePtr<Chunk, ViewWrapper>;

			Ptr Owner;
			std::span<std::byte> Datas;
			mutable Potato::Misc::AtomicRefCount ViewRef;
			mutable Potato::Misc::AtomicRefCount OwnerRef;
		};

	private:

		void NotifyChunkLoseAllViewRef();

		std::size_t ChunkSize;
		std::span<Chunk::OwnerPtr> OwnedPtr;
		
		Page::Ptr PagePtr;
		std::mutex Mutex;
		mutable Potato::Misc::AtomicRefCount Ref;
		friend struct ChunkAllocator;
	};

	struct ChunkAllocator
	{
		struct Wrapper
		{
			void AddRef(ChunkAllocator const* Ptr);
			void SubRef(ChunkAllocator const* Ptr);
		};

		struct Element
		{
			std::size_t ChunkSize = 0;
			ChunkPage::Ptr Ptr;
		};

		using Ptr = Potato::Misc::IntrusivePtr<ChunkAllocator, Wrapper>;

		static Ptr Create(std::size_t MinChunkElementCount = 0);

		//Chunk::ViewPtr GetChunk(std::size_t MinSize);

		static ChunkPage::Chunk::ViewPtr CreateChunk(std::size_t MinChunkSize);

	private:
		
		ChunkAllocator() = default;
		Page::Ptr PagePtr;
		ChunkAllocator::Ptr NextChunk;
		std::size_t AvailableChunkCount;
		mutable Potato::Misc::AtomicRefCount Ref;
		std::mutex Mutex;
		std::span<Element> Elements;
	};
}