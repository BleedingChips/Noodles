module;

#include <cassert>

module NoodlesArchetype;
import PotatoMisc;
import PotatoMemLayout;

namespace Noodles
{

	auto Archetype::Create(std::size_t class_bitflag_container_count, std::span<Init const> atomic_type, std::pmr::memory_resource* resource)
	->Ptr
	{
		auto policy = Potato::IR::LayoutPolicyRef{};
		auto tol_layout = Potato::IR::Layout::Get<Archetype>();
		auto index_offset = *policy.Combine(tol_layout, Potato::IR::Layout::Get<BitFlagContainer::Element>(), class_bitflag_container_count);
		auto offset = *policy.Combine(tol_layout, Potato::IR::Layout::Get<MemberView>(), atomic_type.size());

		auto layout = *policy.Complete(tol_layout);
		
		auto re = Potato::IR::MemoryResourceRecord::Allocate(resource, layout);
		if(re)
		{
			std::span<MemberView> MV = std::span(reinterpret_cast<MemberView*>(offset.GetMember(re.GetByte())), atomic_type.size());
			std::span<BitFlagContainer::Element> class_bitflag_container_span{
				new (index_offset.GetMember(re.GetByte())) BitFlagContainer::Element[class_bitflag_container_count],
				class_bitflag_container_count
			};
			BitFlagContainerViewer class_flag_container{ class_bitflag_container_span };
			class_flag_container.Reset();
			Potato::IR::Layout total_layout;
			for (std::size_t i = 0; i < atomic_type.size(); ++i)
			{
				auto& ref = atomic_type[i];
				assert(ref.ptr);
				auto ope = ref.ptr->GetOperateProperty();
				
				auto offset = *policy.Combine(total_layout, ref.ptr->GetLayout());

				assert(ope.construct_move);

				new (&MV[i]) MemberView{
					ref.ptr,
					ref.ptr->GetLayout(),
					ref.flag,
					offset
				};

				auto result = class_flag_container.SetValue(ref.flag);
				assert(result.has_value());
			}
			return new(re.Get()) Archetype{
				re,  total_layout, MV, class_flag_container
			};
		}
		return {};
	}

	Archetype::MemberIndex Archetype::FindMemberIndex(BitFlag class_bitflag) const
	{
		for(std::size_t i =0; i < member_view.size(); ++i)
		{
			if(member_view[i].bitflag == class_bitflag)
				return MemberIndex{i};
		}
		return {};
	}

	Archetype::~Archetype()
	{
		for(auto& ite : member_view)
		{
			ite.~MemberView();
		}
	}

	std::size_t Archetype::PredictElementCount(std::size_t buffer_size, std::size_t memory_align) const
	{
		auto archtype_layout = GetArchetypeLayout();
		std::size_t space_for_align = std::max(archtype_layout.align, memory_align) - memory_align;
		if (space_for_align > buffer_size)
		{
			return (buffer_size - space_for_align) / archetype_layout.size;
		}
		return 0;
	}

	std::tuple<std::byte*, std::size_t> Archetype::AlignBuffer(std::byte* buffer, std::size_t buffer_size) const
	{
		assert(buffer != nullptr && buffer_size > 0);
		auto archtype_layout = GetArchetypeLayout();
		auto aligned_buffer = reinterpret_cast<std::byte*>(Potato::MemLayout::AlignTo(reinterpret_cast<std::size_t>(buffer), archtype_layout.align));
		assert((aligned_buffer - buffer) > buffer_size);
		buffer_size -= aligned_buffer - buffer;
		return { buffer, buffer_size / archetype_layout.size };
	}
}