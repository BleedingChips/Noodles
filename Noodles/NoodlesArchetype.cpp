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

	std::strong_ordering ArchetypeMountPoint::operator<=>(ArchetypeMountPoint const& mp) const
	{
		return Potato::Misc::PriorityCompareStrongOrdering(
			//index, mp.index,
			buffer, mp.buffer, index, mp.index
		);
	}

	ArchetypeMountPoint::operator bool() const
	{
		return buffer != nullptr;
	}

	/*
	ArchetypeConstructor::ArchetypeConstructor(std::pmr::memory_resource* resource)
		: elements(resource)
	{
		
	}

	
	std::optional<std::size_t> ArchetypeConstructor::AddElement(ArchetypeID const& id)
	{
		if(*this)
		{
			auto find = std::find_if(
				elements.begin(),
				elements.end(),
				[&](Element const& e)
				{
					return id <= e.id;
				}
			);

			if (find != elements.end() && find->id == id)
			{
				if (id.is_singleton)
				{
					status = Status::Bad;
					return std::nullopt;
				}
				else
				{
					auto old = find->count;
					find->count += 1;
					return old;
				}
			}
			else
			{
				elements.insert(
					find,
					Element{ id, 1 }
				);
				return 0;
			}
		}
		return std::nullopt;
	}


	bool ArchetypeConstructor::AddElement(std::span<ArchetypeID const> span, std::size_t& bad_index)
	{
		bad_index = 0;
		for(auto& ite : span)
		{
			if(!AddElement(ite))
				return false;
			++bad_index;
		}
		return true;
	}

	bool ArchetypeConstructor::AddElement(std::span<ArchetypeID const> span)
	{
		for (auto& ite : span)
		{
			if (!AddElement(ite))
				return false;
		}
		return true;
	}

	std::optional<std::size_t> ArchetypeConstructor::Exits(UniqueTypeID const& id) const
	{
		auto find = std::find_if(elements.begin(), elements.end(), [&](Element const& e)
		{
			return e.id.id == id;
		});

		if(find != elements.end())
		{
			return find->count;
		}else
			return std::nullopt;
	}
	*/


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

	void* Archetype::GetData(Element const& ref, ArchetypeMountPoint mp)
	{
		return static_cast<std::byte*>(mp.buffer) + mp.element_count * ref.offset + mp.index * ref.id.layout.Size;
	}

	void* Archetype::GetData(std::size_t locate_index, ArchetypeMountPoint mount_point) const
	{
		assert(GetTypeIDCount() > locate_index);
		assert(mount_point);
		return GetData(infos[locate_index], mount_point);
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

	void Archetype::MoveConstruct(ArchetypeMountPoint target_mp, ArchetypeMountPoint source_mp) const
	{
		for(auto& ite : infos)
		{
			MoveConstruct(ite, target_mp, source_mp);
		}
	}

	void Archetype::Destruct(ArchetypeMountPoint target) const
	{
		for(auto& ite : infos)
		{
			Destruct(ite, target);
		}
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