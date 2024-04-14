module;

#include <cassert>

module NoodlesArchetype;
import PotatoMisc;

namespace Noodles
{
	std::strong_ordering ArchetypeID::operator<=>(ArchetypeID const& i2) const
	{
		return Potato::Misc::PriorityCompareStrongOrdering(
				layout, i2.layout,
			i2.id, id
		);
	}

	bool ArchetypeID::operator==(const ArchetypeID& i1) const
	{
		return layout == i1.layout && id == i1.id;
	}

	auto Archetype::Create(std::span<ArchetypeID const> id, std::pmr::memory_resource* resource)
	->Ptr
	{

		if(resource != nullptr && id.size() >= 1)
		{
			auto layout = Potato::IR::Layout::Get<Archetype>();
			auto list_layout = Potato::IR::Layout::GetArray<Archetype::Element>(id.size());
			auto offset = Potato::IR::InsertLayoutCPP(layout, list_layout);
			Potato::IR::FixLayoutCPP(layout);

			auto record = Potato::IR::MemoryResourceRecord::Allocate(resource, layout);
			if(record)
			{
				auto span = record.GetArray<Archetype::Element>(id.size(), offset);
				for(std::size_t i = 0; i < id.size(); ++i)
				{
					new (&span[i]) Archetype::Element{id[i], 0};
				}

				return new (record.Get()) Archetype {record, span};
			}
		}
		return {};
	}

	Archetype::Ptr Archetype::Clone(std::pmr::memory_resource* o_resource) const
	{
		if(o_resource != nullptr)
		{
			auto layout = Potato::IR::Layout::Get<Archetype>();
			auto list_layout = Potato::IR::Layout::GetArray<Archetype::Element>(infos.size());
			auto offset = Potato::IR::InsertLayoutCPP(layout, list_layout);
			Potato::IR::FixLayoutCPP(layout);
			auto re = Potato::IR::MemoryResourceRecord::Allocate(o_resource, layout);
			if (re)
			{
				auto span = re.GetArray<Element>(infos.size(), offset);
				for(auto ite = span.begin(), ite2 = infos.begin(); ite != span.end(); ++ite, ++ite2)
				{
					new (&*ite) Element{*ite2};
				}
				return new (re.Get()) Archetype{*this, span, record};
			}
		}
		return {};
	}
	
	Archetype::Archetype(
		Potato::IR::MemoryResourceRecord record,
		std::span<Element> infos
		)
		: record(record), infos(infos)
	{
		std::sort(infos.begin(), infos.end(), [](Archetype::Element const& E1, Archetype::Element const& E2)
			{
				return (E1.id.layout <=> E2.id.layout) == std::strong_ordering::greater;
			});

		for(auto& ite : infos)
		{
			ite.offset = Potato::IR::InsertLayoutCPP(archetype_layout, ite.id.layout);
			type_id_hash_code += ite.id.id.HashCode();
		}

		single_layout = archetype_layout;
		Potato::IR::FixLayoutCPP(single_layout);
	}

	Archetype::Archetype(
		Archetype const& ref,
		std::span<Element> infos,
		Potato::IR::MemoryResourceRecord record
	) : record(record), type_id_hash_code(ref.type_id_hash_code),
		single_layout(ref.single_layout), archetype_layout(ref.archetype_layout),
		infos(infos)
	{
	}

	Archetype::~Archetype()
	{
		for (auto& ite : infos)
		{
			ite.~Element();
		}
		infos = {};
	}

	void Archetype::Release()
	{
		auto re = record;
		this->~Archetype();
		re.Deallocate();
	}

	auto Archetype::LocateTypeID(UniqueTypeID const& type_id) const
		-> std::optional<std::size_t>
	{
		for(auto ite = infos.begin(); ite != infos.end(); ++ite)
		{
			if(type_id == ite->id.id)
			{
				return static_cast<std::size_t>(std::distance(infos.begin(), ite));
			}
		}
		return std::nullopt;
	}

	UniqueTypeID const& Archetype::GetTypeID(std::size_t index) const
	{
		assert(GetTypeIDCount() > index);
		return infos[index].id.id;
	}

	void* Archetype::Get(RawArray raw_data, std::size_t array_index)
	{
		assert(raw_data.array_count > array_index);
		return static_cast<std::byte*>(raw_data.buffer) + array_index * raw_data.element_layout.Size;
	}

	Archetype::RawArray Archetype::Get(Element const& ref, ArrayMountPoint mount_point)
	{
		return RawArray{
			static_cast<std::byte*>(mount_point.GetBuffer()) + ref.offset * mount_point.total_count,
			mount_point.available_count,
			ref.id.layout
		};
	}

	bool Archetype::CheckUniqueArchetypeID(std::span<ArchetypeID const> ids)
	{
		for(auto ite = ids.begin(); ite != ids.end(); ++ite)
		{
			for(auto ite2 = ite + 1; ite2 != ids.end(); ++ite2)
			{
				if(*ite == *ite2)
					return false;
			}
		}
		return true;
	}

	bool Archetype::operator==(Archetype const& i2) const
	{
		if(type_id_hash_code == i2.type_id_hash_code && archetype_layout == i2.archetype_layout)
		{
			if(infos.size() == i2.infos.size())
			{
				for(auto ite = infos.begin(), ite2 = i2.infos.begin(); ite != infos.end(); ++ite, ++ite2)
				{
					if(*ite != *ite2)
						return false;
				}
				return true;
			}
		}
		return false;
	}

}