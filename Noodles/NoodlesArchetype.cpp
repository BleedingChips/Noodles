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
		auto tol_layout = Potato::MemLayout::MemLayoutCPP::Get<Archetype>();
		auto index_offset = tol_layout.Insert(Potato::IR::Layout::GetArray<BitFlagContainer::Element>(class_bitflag_container_count));
		auto offset = tol_layout.Insert(Potato::IR::Layout::GetArray<MemberView>(atomic_type.size()));

		auto layout = tol_layout.Get();
		
		auto re = Potato::IR::MemoryResourceRecord::Allocate(resource, layout);
		if(re)
		{
			std::span<MemberView> MV = std::span(reinterpret_cast<MemberView*>(re.GetByte() + offset), atomic_type.size());
			std::span<BitFlagContainer::Element> class_bitflag_container_span{
				new (re.GetByte(index_offset)) BitFlagContainer::Element[class_bitflag_container_count],
				class_bitflag_container_count
			};
			BitFlagContainerViewer class_flag_container{ class_bitflag_container_span };
			class_flag_container.Reset();
			Potato::MemLayout::MemLayoutCPP total_layout;
			for (std::size_t i = 0; i < atomic_type.size(); ++i)
			{
				auto& ref = atomic_type[i];
				assert(ref.ptr);
				auto ope = ref.ptr->GetOperateProperty();
				
				std::size_t offset = total_layout.Insert(ref.ptr->GetLayout());

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
			auto archetype_layout = total_layout.GetRawLayout();
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
}