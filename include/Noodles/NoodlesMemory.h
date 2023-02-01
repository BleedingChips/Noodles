#pragma once
#include "Potato/PotatoMisc.h"
#include "Potato/PotatoIntrusivePointer.h"
#include <mutex>

namespace Noodles::Memory
{

	struct HugePage
	{
		using PtrT = Potato::Misc::IntrusivePtr<HugePage>;

		static auto Create(std::size_t MinSize) -> PtrT;

		std::span<std::byte> GetBuffer() const { return Buffer; }
		void AddRef() const { Ref.AddRef(); }
		
		template<typename Func>
		void SubRef(Func Fun) const { if(Ref.SubRef()) { Fun(); ReleaseExe(); }};

		void SubRef() const;

	private:

		HugePage(std::span<std::byte> Buffer) : Buffer(Buffer) {}

		void ReleaseExe() const;

		mutable Potato::Misc::AtomicRefCount Ref;
		std::span<std::byte> Buffer;

	};

	struct PageManager
	{
		using PtrT = Potato::Misc::IntrusivePtr<PageManager>;

		static auto CreateInstance(std::size_t MaxCacheSize = 10) -> PtrT;

		struct Page
		{
			using PtrT = Potato::Misc::IntrusivePtr<Page>;
			void AddRef() {}
			void SubRef() {}
		};

		void AddRef() const { Ref.AddRef(); }

		void SubRef() const;

	private:

		PageManager() = default;

		std::span<Page::PtrT> Pages;
		std::span<HugePage::PtrT> CachedPage;

		mutable Potato::Misc::AtomicRefCount Ref;
		HugePage::PtrT Owner;
	};

}