module;

#include <cassert>

module NoodlesArchetype;
import PotatoMisc;
import PotatoMemLayout;

namespace Noodles
{

	auto Archetype::Create(StructLayoutMarkIndexManager& manager, std::span<Init const> atomic_type, std::pmr::memory_resource* resource)
	->Ptr
	{
		auto storage_count = manager.GetStorageCount();
		auto tol_layout = Potato::MemLayout::MemLayoutCPP::Get<Archetype>();
		auto index_offset = tol_layout.Insert(Potato::IR::Layout::GetArray<MarkElement>(storage_count));
		auto offset = tol_layout.Insert(Potato::IR::Layout::GetArray<MemberView>(atomic_type.size()));

		auto layout = tol_layout.Get();
		
		auto re = Potato::IR::MemoryResourceRecord::Allocate(resource, layout);
		if(re)
		{
			std::span<MemberView> MV = std::span(reinterpret_cast<MemberView*>(re.GetByte() + offset), atomic_type.size());
			std::span<MarkElement> archetype_index{
				new (re.GetByte(index_offset)) MarkElement[storage_count],
				storage_count
			};
			;
			Potato::MemLayout::MemLayoutCPP total_layout;
			for (std::size_t i = 0; i < atomic_type.size(); ++i)
			{
				auto& ref = atomic_type[i];
				assert(ref.ptr);
				auto ope = ref.ptr->GetOperateProperty();
				
				std::size_t offset = total_layout.Insert(ref.ptr->GetLayout());

				assert(ope.default_construct && ope.move_construct);

				new (&MV[i]) MemberView{
					ref.ptr,
					ref.index,
					offset,
				};

				MarkElement::Mark(archetype_index, ref.index);
			}
			auto archetype_layout = total_layout.GetRawLayout();
			return new(re.Get()) Archetype{
				re,  total_layout, MV, archetype_index
			};
		}
		return {};
	}

	std::optional<Archetype::Index> Archetype::Locate(MarkIndex id) const
	{
		for(std::size_t i =0; i < member_view.size(); ++i)
		{
			if(member_view[i].index == id)
				return Index{i};
		}
		return std::nullopt;
	}

	Archetype::~Archetype()
	{
		for(auto& ite : member_view)
		{
			ite.~MemberView();
		}
	}

}