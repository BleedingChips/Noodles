module;

#include <cassert>

module NoodlesArchetype;
import PotatoMisc;

namespace Noodles
{
	std::strong_ordering ArchetypeID::operator<=>(ArchetypeID const& i2) const
	{
		return Potato::Misc::PriorityCompareStrongOrdering(
			layout.Align, i2.layout.Align,
			layout.Size, i2.layout.Size,
			i2.id, id
		);
	}

	std::strong_ordering ArchetypeMountPoint::operator<=>(ArchetypeMountPoint const& mp) const
	{
		auto re = index <=> mp.index;
		if(re == std::strong_ordering::equivalent)
		{
			re = buffer <=> mp.buffer;
		}
		return re;
	}

	ArchetypeMountPoint::operator bool() const
	{
		return buffer != nullptr && element_count >= 1 && index < element_count;
	}

	ArchetypeConstructor::ArchetypeConstructor(std::pmr::memory_resource* resource)
		: elements(resource)
	{
		
	}

	
	bool ArchetypeConstructor::AddElement(ArchetypeID const& id)
	{
		if(*this)
		{
			auto find = std::find_if(
				elements.begin(),
				elements.end(),
				[&](Element const& e)
				{
					return (id <=> e.id) != std::strong_ordering::greater;
				}
			);

			if (find != elements.end() && (find->id <=> id) == std::strong_ordering::equivalent)
			{
				if (id.is_singleton)
				{
					status = Status::Bad;
					return false;
				}
				else
				{
					find->count += 1;
					return true;
				}
			}
			else
			{
				elements.insert(
					find,
					Element{ id, 1 }
				);
				return true;
			}
		}
		return false;
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
			return (e.id.id <=> id) == std::strong_ordering::equivalent;
		});

		if(find != elements.end())
		{
			return find->count;
		}else
			return std::nullopt;
	}


	auto Archetype::Create(ArchetypeConstructor const& ref_info, std::pmr::memory_resource* resource)
	->Ptr
	{
		assert(resource != nullptr);

		if(ref_info)
		{
			auto info_size = ref_info.elements.size() * sizeof(Element);
			auto total_size = info_size + sizeof(Archetype);
			auto adress = resource->allocate(total_size, alignof(Archetype));
			if (adress != nullptr)
			{
				Ptr ptr = new (adress) Archetype{
					ref_info,
					std::span<std::byte>{static_cast<std::byte*>(adress) + sizeof(Archetype), info_size},
					resource,
					total_size
				};
				
				assert(ptr);
				return ptr;
			}
		}
		return {};
	}
	
	Archetype::Archetype(
		ArchetypeConstructor const& contructor,
		std::span<std::byte> buffer, 
		std::pmr::memory_resource* resouce, 
		std::size_t allocated_size)
		: resource(resouce), allocated_size(allocated_size)
	{
		assert(contructor.elements.size() * sizeof(Element) <= buffer.size());
		std::span<Element> temp_buffer{
			reinterpret_cast<Element*>(buffer.data()),
			contructor.elements.size()
		};

		infos = temp_buffer;

		std::size_t index = 0;
		for (auto& ite : contructor.elements)
		{
			assert(index < temp_buffer.size());
			new (&temp_buffer[index]) Element{
				ite.id,
				ite.count
			};
			++index;
		}

		std::sort(temp_buffer.begin(), temp_buffer.end(), [](Element const& E, Element const& E2)
			{
				auto re = E.id <=> E2.id;
				if (re != std::strong_ordering::less)
					return true;
				return false;
			});

		Potato::IR::Layout layout;

		for (auto& ite : temp_buffer)
		{
			auto counted_layout = ite.id.layout;
			counted_layout.Size *= ite.count;
			ite.offset = Potato::IR::InsertLayoutCPP(layout, counted_layout);
		}

		infos = temp_buffer;

		buffer_archetype_layout_size = layout.Size;

		Potato::IR::FixLayoutCPP(layout);

		archetype_layout = layout;
	}

	Archetype::Archetype(
		Archetype const& ref,
		std::span<std::byte> buffer,
		std::pmr::memory_resource* resource, std::size_t allocated_size
	) : resource(resource), allocated_size(allocated_size)
	{
		assert(ref.infos.size() * sizeof(Element) <= buffer.size());
		std::span<Element> new_infos{
			reinterpret_cast<Element*>(buffer.data()),
			ref.infos.size()
		};
		std::size_t count = 0;
		for (auto& ite : ref.infos)
		{
			new (&new_infos[count]) Element{ ite };
			count += 1;
		}
		archetype_layout = ref.archetype_layout;
		infos = new_infos;
		buffer_archetype_layout_size = ref.buffer_archetype_layout_size;
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
		auto old_resource = resource;
		auto old_size = allocated_size;
		this->~Archetype();
		old_resource->deallocate(
			this,
			old_size,
			alignof(Archetype)
		);
	}

	auto Archetype::LocateTypeID(UniqueTypeID const& type_id) const
		-> std::optional<Location>
	{
		std::size_t index = 0;
		for(auto& ite : infos)
		{
			auto re = type_id <=> ite.id.id;
			if (re == std::strong_ordering::equal)
			{
				return Location{index, ite.count};
			}
			++index;
		}
		return std::nullopt;
	}

	UniqueTypeID const& Archetype::GetTypeID(std::size_t index) const
	{
		assert(GetTypeIDCount() > index);
		return infos[index].id.id;
	}

	UniqueTypeID const& Archetype::GetTypeID(std::size_t index, std::size_t& count) const
	{
		assert(GetTypeIDCount() > index);
		auto& ref = infos[index];
		count = ref.count;
		return ref.id.id;
	}

	void* Archetype::GetData(std::size_t locate_index, std::size_t count, ArchetypeMountPoint mount_point) const
	{
		assert(GetTypeIDCount() > locate_index);
		assert(mount_point);
		auto& ref = infos[locate_index];
		assert(count < ref.count);
		auto layout = ref.id.layout;
		auto offset = ref.offset;
		return static_cast<void*>(
			static_cast<std::byte*>(mount_point.buffer) 
			+ offset * mount_point.element_count 
			+ layout.Size * (mount_point.index * ref.count + count)
			);
	}

	void Archetype::Destruction(ArchetypeMountPoint mount_point) const
	{
		assert(mount_point);
		for(auto& ite : infos)
		{
			auto layout = ite.id.layout;
			auto offset = ite.offset;

			auto buffer_offset = offset * mount_point.element_count
				+ layout.Size * (mount_point.index * ite.count);

			for(std::size_t count = 0; count < ite.count; ++count)
			{
				
				auto target_adress = static_cast<void*>(
					static_cast<std::byte*>(mount_point.buffer) + 
					buffer_offset + layout.Size * count
					);
				ite.id.wrapper_function(ArchetypeID::Status::Destruction, target_adress, nullptr);
			}
		}
	}

	void Archetype::MoveConstruct(ArchetypeMountPoint target_mp, ArchetypeMountPoint source_mp) const
	{
		for (auto& ite : infos)
		{
			auto layout = ite.id.layout;
			auto offset = ite.offset;

			auto buffer_offset1 = offset * target_mp.element_count
				+ layout.Size * (target_mp.index * ite.count);

			auto buffer_offset2 = offset * source_mp.element_count
				+ layout.Size * (source_mp.index * ite.count);

			for (std::size_t count = 0; count < ite.count; ++count)
			{
				auto target_adress1 = static_cast<void*>(
					static_cast<std::byte*>(target_mp.buffer) +
					buffer_offset1 + layout.Size * count
					);
				auto target_adress2 = static_cast<void*>(
					static_cast<std::byte*>(source_mp.buffer) +
					buffer_offset2 + layout.Size * count
					);
				ite.id.wrapper_function(ArchetypeID::Status::MoveConstruction, target_adress1, target_adress2);
			}
		}
	}

	void Archetype::MoveConstruct(std::size_t locate_index, void* target, void* source) const
	{
		assert(GetTypeIDCount() > locate_index);
		assert(target != nullptr && source != nullptr);
		auto& ref = infos[locate_index];
		ref.id.wrapper_function(ArchetypeID::Status::MoveConstruction, target, source);
	}

	void Archetype::Destruction(std::size_t locate_index, void* target) const
	{
		assert(GetTypeIDCount() > locate_index);
		assert(target != nullptr);
		auto& ref = infos[locate_index];
		ref.id.wrapper_function(ArchetypeID::Status::Destruction, target, nullptr);
	}

	void Archetype::DefaultConstruct(std::size_t locate_index, void* target) const
	{
		assert(GetTypeIDCount() > locate_index);
		assert(target != nullptr);
		auto& ref = infos[locate_index];
		ref.id.wrapper_function(ArchetypeID::Status::DefaultConstruction, target, nullptr);
	}

	Archetype::Ptr Archetype::Clone(std::pmr::memory_resource* o_resource) const
	{
		if(o_resource != nullptr)
		{
			auto new_adress = o_resource->allocate(allocated_size, alignof(Archetype));
			if(new_adress != nullptr)
			{
				Ptr ptr = new (new_adress) Archetype{
					*this,
					std::span<std::byte>{ reinterpret_cast<std::byte*>(new_adress) + sizeof(Archetype), sizeof(Element) * infos.size()},
					o_resource, allocated_size };

				assert(ptr);
				return ptr;
			}
		}
		return {};
	}

	std::strong_ordering Archetype::operator<=>(Archetype const& i2) const
	{
		auto re = Potato::Misc::PriorityCompareStrongOrdering(
			infos.size(), i2.infos.size(),
			archetype_layout.Align, i2.archetype_layout.Align,
			archetype_layout.Size, i2.archetype_layout.Size
		);

		if(re == std::strong_ordering::equivalent)
		{
			for (std::size_t i = 0; i < infos.size(); ++i)
			{
				auto& it1 = infos[i];
				auto& it2 = i2.infos[i];
				re = Potato::Misc::PriorityCompareStrongOrdering(
					it1.id, it2.id,
					it1.count, it2.count
				);
				if (re != std::strong_ordering::equivalent)
					return re;
			}
		}
		return re; 
	}

}