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
			Potato::MemLayout::MermberLayout member_layout;
			std::byte* GetMember(std::size_t element_count, std::byte* buffer, std::size_t array_index = 0) const 
			{
				return member_layout.array_layout.GetElement(buffer + member_layout.offset * element_count, array_index);
			}
			std::byte const* GetMember(std::size_t element_count, std::byte const* buffer, std::size_t array_index = 0) const
			{
				return GetMember(element_count, const_cast<std::byte*>(buffer), array_index);
			}
			std::byte* GetMember(std::size_t element_count, void* buffer, std::size_t array_index = 0) const
			{
				return GetMember(element_count, static_cast<std::byte*>(buffer), array_index);
			}
			std::byte const* GetMember(std::size_t element_count, void const* buffer, std::size_t array_index = 0) const
			{
				return GetMember(element_count, static_cast<std::byte const*>(buffer), array_index);
			}
		};

		using MemberIndex = OptionalSizeT;

		using Ptr = Potato::Pointer::IntrusivePtr<Archetype>;
		using OPtr = Potato::Pointer::ObserverPtr<Archetype const>;

		struct Init
		{
			Potato::IR::StructLayout::Ptr ptr;
			BitFlag flag;
		};

		static Ptr Create(std::size_t class_bitflag_container_count, std::span<Init const> atomic_type, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		MemberIndex FindMemberIndex(BitFlag class_bitflag) const;
		std::size_t GetAtomicTypeCount() const { return GetMemberView().size(); }

		Potato::IR::Layout GetLayout() const { Potato::IR::LayoutPolicyRef policy; return *policy.Complete(archetype_layout); }
		Potato::IR::Layout GetArchetypeLayout() const { return archetype_layout; }

		std::span<MemberView const> GetMemberView() const { return member_view; }
		MemberView const& GetMemberView(Archetype::MemberIndex index) const { return member_view[index.Get()]; };

		decltype(auto) begin() const { return member_view.begin(); }
		decltype(auto) end() const { return member_view.end(); }
		MemberView const& operator[](std::size_t index) const { return member_view[index]; }
		BitFlagContainerConstViewer GetClassBitFlagContainer() const { return class_bitflag_container; }

		std::size_t PredictElementCount(std::size_t buffer_size, std::size_t memory_align = alignof(std::nullptr_t)) const;
		std::tuple<std::byte*, std::size_t> AlignBuffer(std::byte* buffer, std::size_t buffer_size) const;

	protected:

		Archetype(
			Potato::IR::MemoryResourceRecord record,
			Potato::IR::Layout archetype_layout,
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

		Potato::IR::Layout archetype_layout;
		std::span<MemberView const> member_view;

		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
	};

}