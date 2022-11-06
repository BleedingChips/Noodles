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

	struct Chunk
	{
		struct Wrapper
		{
			void AddRef(ChunkManager const* Ptr);
			void SubRef(ChunkManager const* Ptr);
		};

		using Ptr = Potato::Misc::IntrusivePtr<ChunkManager, Wrapper>;

	private:
		
		Page::Ptr PagePtr;
		std::mutex Mutex;
	};

	struct ChunkManager
	{
		struct Wrapper
		{
			void AddRef(ChunkManager const* Ptr);
			void SubRef(ChunkManager const* Ptr);
		};

		using Ptr = Potato::Misc::IntrusivePtr<ChunkManager, Wrapper>;

		static Ptr Create();

	private:
		
		ChunkManager() = default;
		Page::Ptr PagePtr;
		ChunkManager::Ptr NextChunk;
		mutable Potato::Misc::AtomicRefCount Count;
		std::mutex Mutex;
	};
}