module;

#include <cassert>

module NoodlesArchetype;
import PotatoMisc;

namespace Noodles
{

	std::optional<std::size_t> Archetype::CalculateHashCode(std::span<AtomicType::Ptr> atomic_type)
	{
		std::size_t hash_code = 0;
		for(auto& ite : atomic_type)
		{
			if(ite)
			{
				hash_code += ite->GetHashCode();
			}else
			{
				return std::nullopt;
			}
		}
		return hash_code;
	}


	auto Archetype::Create(std::span<AtomicType::Ptr> atomic_type, std::pmr::memory_resource* resource)
	->Ptr
	{
		auto hash_code = CalculateHashCode(atomic_type);
		if(!hash_code)
			return {};
		auto layout = Potato::IR::Layout::Get<Archetype>();
		auto offset = Potato::IR::InsertLayoutCPP(layout, Potato::IR::Layout::GetArray<MemberView>(atomic_type.size()));
		Potato::IR::FixLayoutCPP(layout);

		auto re = Potato::IR::MemoryResourceRecord::Allocate(resource, layout);
		if(re)
		{
			std::span<MemberView> MV = std::span(reinterpret_cast<MemberView*>(re.GetByte() + offset), atomic_type.size());
			StructLayoutConstruction construct;
			Potato::IR::Layout total_layout;
			for(std::size_t i = 0; i < atomic_type.size(); ++i)
			{
				auto& ref = atomic_type[i];
				assert(ref);
				construct = construct && ref->GetConstructProperty();
				std::size_t offset = Potato::IR::InsertLayoutCPP(total_layout, ref->GetLayout());
				new (&MV[i]) MemberView{
					ref,
					{},
					1,
					offset
				};
			}
			auto archetype_layout = total_layout;
			Potato::IR::FixLayoutCPP(total_layout);
			return new(re.Get()) Archetype {
				re,  total_layout,archetype_layout, MV, *hash_code, construct};
		}
		return {};
	}

	void Archetype::Release()
	{
		auto re = record;
		auto infos = member_view;
		this->~Archetype();
		for(auto& ite : infos)
		{
			ite.~MemberView();
		}
		re.Deallocate();
	}

	auto Archetype::LocateTypeID(AtomicType const& type_id) const
		-> std::optional<std::size_t>
	{
		for(std::size_t i =0; i < member_view.size(); ++i)
		{
			if(*member_view[i].struct_layout == type_id)
				return i;
		}
		return std::nullopt;
	}

	AtomicType::Ptr Archetype::GetTypeID(std::size_t index) const
	{
		if(member_view.size() > index)
		{
			return member_view[index].struct_layout;
		}
		return {};
	}

	void* Archetype::Get(RawArray raw_data, std::size_t array_index)
	{
		assert(raw_data.array_count > array_index);
		return static_cast<std::byte*>(raw_data.buffer) + array_index * raw_data.element_layout.Size;
	}

	Archetype::RawArray Archetype::Get(MemberView const& ref, ArrayMountPoint mount_point)
	{
		return RawArray{
			static_cast<std::byte*>(mount_point.GetBuffer()) + ref.offset * mount_point.total_count,
			mount_point.available_count,
			ref.struct_layout->GetLayout()
		};
	}

}