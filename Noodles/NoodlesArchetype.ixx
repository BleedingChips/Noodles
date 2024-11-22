module;

#include <cassert>
export module NoodlesArchetype;

import std;
import PotatoIR;
import PotatoPointer;
export import NoodlesMisc;

export namespace Noodles
{

	struct Archetype : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{

		struct MemberView
		{
			Potato::IR::StructLayout::Ptr layout;
			MarkIndex index;
			std::size_t offset;
		};

		struct Index
		{
			std::size_t index = std::numeric_limits<std::size_t>::max();
			std::strong_ordering operator <=>(Index const&) const = default;
			bool operator ==(Index const&) const = default;
		};

		using Ptr = Potato::Pointer::IntrusivePtr<Archetype>;
		using OPtr = Potato::Pointer::ObserverPtr<Archetype const>;

		struct Init
		{
			StructLayout::Ptr ptr;
			MarkIndex index;
		};

		static Ptr Create(StructLayoutMarkIndexManager& manager, std::span<Init const> atomic_type, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		std::optional<Archetype::Index> Locate(MarkIndex index) const;
		std::size_t GetAtomicTypeCount() const { return GetMemberView().size(); }

		Potato::IR::Layout GetLayout() const { return archetype_layout.Get(); }
		Potato::IR::Layout GetArchetypeLayout() { return archetype_layout.GetRawLayout(); }

		std::span<MemberView const> GetMemberView() const { return member_view; }
		MemberView const& GetMemberView(Archetype::Index index) const { return member_view[index.index]; };

		decltype(auto) begin() const { return member_view.begin(); }
		decltype(auto) end() const { return member_view.end(); }
		MemberView const& operator[](std::size_t index) const { return member_view[index]; }
		std::span<MarkElement const> GetAtomicTypeMark() const { return archetype_mark; }

	protected:

		Archetype(
			Potato::IR::MemoryResourceRecord record,
			Potato::MemLayout::MemLayoutCPP archetype_layout,
			std::span<MemberView const> member_view,
			std::span<MarkElement> archetype_mark
			)
				: MemoryResourceRecordIntrusiveInterface(record),
			archetype_layout(archetype_layout),
			member_view(member_view),
			archetype_mark(archetype_mark)
		{
			
		}

		std::span<MarkElement> archetype_mark;

		~Archetype();

		Potato::MemLayout::MemLayoutCPP archetype_layout;
		std::span<MemberView const> member_view;

		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
	};

}