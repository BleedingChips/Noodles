#pragma once
#include "Potato/PotatoMisc.h"
#include "Potato/PotatoIntrusivePointer.h"
#include <mutex>

namespace Noodles::Memory
{
	struct PageManager : public Potato::Misc::DefaultIntrusiveObjectInterface<PageManager>
	{
		using PtrT = Potato::Misc::IntrusivePtr<PageManager>;

		static auto Create() ->PtrT { return new PageManager{}; }


		struct Page
		{
			std::span<std::byte> GetBuffer() const { return Buffer; }
			void AddRef() const { Ref.AddRef(); }
			void SubRef() const;
		protected:
			~Page();
			Page(PtrT Owner) : Owner(std::move(Owner)) {};
			mutable Potato::Misc::AtomicRefCount Ref;
			std::span<std::byte> Buffer;
			PtrT Owner;

			friend struct PageManager;
		};

		using PPtrT = Potato::Misc::IntrusivePtr<Page>;

		PPtrT Allocate(std::size_t MinSize);
		
	protected:

		void Release(Page const* Page);
		friend struct Page;

		PageManager() = default;
	};


}