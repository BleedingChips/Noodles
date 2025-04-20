module;

#include <cassert>
export module NoodlesArchetype;

import std;
import Potato;
import NoodlesBitFlag;

export namespace Noodles
{

	struct Archetype : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{

		struct MemberView
		{
			Potato::IR::StructLayout::Ptr struct_layout;
			Potato::IR::Layout layout;
			BitFlag bitflag;
			std::size_t offset;
		};

		using MemberIndex = OptionalSizeT;

		using Ptr = Potato::Pointer::IntrusivePtr<Archetype>;
		using OPtr = Potato::Pointer::ObserverPtr<Archetype const>;

		struct Init
		{
			StructLayout::Ptr ptr;
			BitFlag flag;
		};

		static Ptr Create(std::size_t class_bitflag_container_count, std::span<Init const> atomic_type, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		MemberIndex FindMemberIndex(BitFlag class_bitflag) const;
		std::size_t GetAtomicTypeCount() const { return GetMemberView().size(); }

		Potato::IR::Layout GetLayout() const { return archetype_layout.Get(); }
		Potato::IR::Layout GetArchetypeLayout() const { return archetype_layout.GetRawLayout(); }

		std::span<MemberView const> GetMemberView() const { return member_view; }
		MemberView const& GetMemberView(Archetype::MemberIndex index) const { return member_view[index.Get()]; };

		decltype(auto) begin() const { return member_view.begin(); }
		decltype(auto) end() const { return member_view.end(); }
		MemberView const& operator[](std::size_t index) const { return member_view[index]; }
		BitFlagContainerConstViewer GetClassBitFlagContainer() const { return class_bitflag_container; }

	protected:

		Archetype(
			Potato::IR::MemoryResourceRecord record,
			Potato::MemLayout::MemLayoutCPP archetype_layout,
			std::span<MemberView const> member_view,
			BitFlagContainerViewer class_bitflag_container
			)
				: MemoryResourceRecordIntrusiveInterface(record),
			archetype_layout(archetype_layout),
			member_view(member_view),
			class_bitflag_container(class_bitflag_container)
		{
			
		}

		BitFlagContainerViewer class_bitflag_container;

		~Archetype();

		Potato::MemLayout::MemLayoutCPP archetype_layout;
		std::span<MemberView const> member_view;

		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
	};

}