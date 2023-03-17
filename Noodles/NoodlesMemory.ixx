module;

export module Noodles.Memory;
export import Potato.SmartPtr;

export namespace Noodles::Memory
{

	struct AllocatorInterfaceT
	{
		using PtrT = Potato::Misc::IntrusivePtr<AllocatorInterfaceT>;

		virtual void AddRef() const = 0;
		virtual void SubRef() const = 0;
		virtual std::byte* allocate(std::size_t ByteCount) = 0;
		virtual void deallocate(std::byte* Adress, std::size_t ByteCount) = 0;
	};

	struct AllocatorT
	{
		AllocatorT() = default;
		AllocatorT(AllocatorInterfaceT::PtrT Ptr) : Ptr(std::move(Ptr)) {}
		AllocatorT(AllocatorT const&) = default;
		AllocatorT(AllocatorT&&) = default;
		std::byte* allocate(std::size_t ByteCount);
		void deallocate(std::byte* Adress, std::size_t ByteCount);
	protected:
		AllocatorInterfaceT::PtrT Ptr;
	};

	struct HugePageT
	{
		using PtrT = Potato::Misc::IntrusivePtr<HugePageT>;

		static auto CreateInstance(std::size_t MinSize, AllocatorT Allocator) -> PtrT;

		std::span<std::byte> GetBuffer() const { return Buffer; }

		void AddRef() const { Ref.AddRef(); }
		void SubRef() const;

	private:

		HugePageT(AllocatorT Allocator, std::span<std::byte> Buffer) : Allocator(std::move(Allocator)), Buffer(Buffer) {}

		AllocatorT Allocator;
		mutable Potato::Misc::AtomicRefCount Ref;
		std::span<std::byte> const Buffer;

	};


	struct ChunkPageT
	{
		using PtrT = Potato::Misc::IntrusivePtr<ChunkPageT>;

		struct ChunkT
		{
			using PtrT = Potato::Misc::IntrusivePtr<ChunkT>;

			void AddRef() const { Ref.AddRef(); };
			void SubRef() const { if (Ref.SubRef()) { Release(); } }

			template<typename Func>
			void SubRef(Func Fun) const
			{
				if (Ref.SubRef())
				{
					Func();
					Release();
				}
			}

			std::span<std::byte> GetBuffer() const { return Buffer; }

		protected:

			void Release() const;

			bool BeingUsed = false;
			std::size_t Index = 0;
			std::size_t UsedChunkStatusCount = 0;
			mutable Potato::Misc::AtomicRefCount Ref;
			std::span<std::byte> Buffer;
			ChunkPageT::PtrT Owner;

			friend struct ChunkPageT;
		};

		static auto CreateInstance(HugePageT::PtrT PagePtr, std::size_t MinChunkSize = 128) -> PtrT {
			if (PagePtr)
			{
				auto Span = PagePtr->GetBuffer();
				return CreateInstance(std::move(PagePtr), Span, MinChunkSize);
			}
			return {};
		}

		static auto CreateInstance(HugePageT::PtrT PagePtr, std::span<std::byte> UsedBuffer, std::size_t MinChunkSize = 128) -> PtrT;
		static auto CreateInstance(std::size_t MinChunkSize = 128, std::size_t MinPageSize = 1024, AllocatorT Allocator = {}) -> PtrT;

		void AddRef() const { Ref.AddRef(); }
		void SubRef() const;

		auto TryAllocate(std::size_t RequireSize) -> ChunkT::PtrT;

	protected:

		ChunkPageT(HugePageT::PtrT Owner, std::size_t ChunkSize, std::byte* Buffer, std::size_t ChunksCount);
		~ChunkPageT();

		HugePageT::PtrT Owner;
		mutable Potato::Misc::AtomicRefCount Ref;
		std::mutex Mutex;
		std::size_t const ChunkSize = 0;
		std::span<ChunkT> const Chunks;
		std::byte* ChunkDataAdress;

		void Release(std::size_t ChunkIndex);
	};

	struct ChunkPageManagerT
	{
		using PtrT = Potato::Misc::IntrusivePtr<ChunkPageT>;
	protected:
		std::mutex Mutex;
		PtrT NextManager;
		Potato::Misc::AtomicRefCount Ref;

	};


}