module;

export module NoodlesMemory;

import std;
import PotatoPointer;
import PotatoMisc;
import PotatoIR;


export namespace Noodles::Memory
{

	struct HugePageMemoryResource : public std::pmr::memory_resource, public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<HugePageMemoryResource>;

		//static std::size_t GetPageSize();

		static Ptr Create(std::pmr::memory_resource* UpResource = std::pmr::get_default_resource());

	protected:

		HugePageMemoryResource(std::pmr::memory_resource* UpStreamResource = std::pmr::get_default_resource());

		virtual void* do_allocate(size_t _Bytes, size_t _Align) override;
		virtual void do_deallocate(void* _Ptr, size_t _Bytes, size_t _Align) override;
		virtual bool do_is_equal(const memory_resource& _That) const noexcept override;

		virtual void Release() override;

		std::pmr::synchronized_pool_resource PoolResource;
	};

	/*


	struct HugePageT
	{
		static auto CreateInstance(std::size_t MinSize, std::pmr::memory_resource* Resource) -> PtrT;

		std::span<std::byte> GetBuffer() const { return Buffer; }

		void AddRef() const { Ref.AddRef(); }
		void SubRef() const;

	private:

		HugePageT(std::pmr::memory_resource* Resource, std::span<std::byte> Buffer) : Resource(Resource), Buffer(Buffer) {}

		std::pmr::memory_resource* Resource;
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

*/
}