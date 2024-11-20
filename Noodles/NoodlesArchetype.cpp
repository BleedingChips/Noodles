module;

#include <cassert>

module NoodlesArchetype;
import PotatoMisc;
import PotatoMemLayout;

namespace Noodles
{

	std::optional<AtomicTypeID> AtomicTypeManager::LocateAtomicType_AssumedLocked(AtomicType::Ptr const& type) const
	{
		if(type)
		{
			for (std::size_t i = 0; i < atomic_type.size(); ++i)
			{
				auto& ref = atomic_type[i];
				if (ref == type || *ref == *type)
				{
					return AtomicTypeID{i};
				}
			}
		}
		return std::nullopt;
	}

	std::optional<AtomicTypeID> AtomicTypeManager::LocateOrAddAtomicType(AtomicType::Ptr const& type)
	{
		{
			std::shared_lock sl(mutex);
			auto re = LocateAtomicType_AssumedLocked(type);
			if(re.has_value())
			{
				return re;
			}
		}

		{
			std::lock_guard lg(mutex);
			auto re = LocateAtomicType_AssumedLocked(type);
			if (re.has_value())
			{
				return re;
			}
			auto count = atomic_type.size();
			if(count < GetMaxAtomicTypeCount())
			{
				atomic_type.emplace_back(type);
				return AtomicTypeID{count};
			}
		}
		return std::nullopt;
	}


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

	std::optional<bool> AtomicTypeMark::Mark(std::span<AtomicTypeMark> marks, AtomicTypeID index, bool mark)
	{
		std::size_t i = index.index / 64;
		std::size_t o = index.index % 64;
		if (i < marks.size())
		{
			auto mark_value = (std::uint64_t{ 1 } << o);
			auto old_value = marks[i].mark;
			marks[i].mark |= mark_value;
			return (old_value & mark_value) == mark_value;
		}
		return std::nullopt;
	}

	std::optional<bool> AtomicTypeMark::CheckIsMark(std::span<AtomicTypeMark const> marks, AtomicTypeID index)
	{
		std::size_t i = index.index / 64;
		std::size_t o = index.index % 64;
		if(i < marks.size())
		{
			auto mark_value = (std::uint64_t{ 1 } << o);
			return (marks[i].mark & mark_value) == mark_value;
		}
		return std::nullopt;
	}

	bool AtomicTypeMark::Inclusion(std::span<AtomicTypeMark const> source, std::span<AtomicTypeMark const> target)
	{
		assert(source.size() == target.size());
		for (std::size_t i = 0; i < target.size(); ++i)
		{
			if ((source[i].mark & target[i].mark) != target[i].mark)
			{
				return false;
			}
		}
		return true;
	}

	bool AtomicTypeMark::IsOverlapping(std::span<AtomicTypeMark const> source, std::span<AtomicTypeMark const> target)
	{
		assert(source.size() == target.size());
		for (std::size_t i = 0; i < target.size(); ++i)
		{
			if ((source[i].mark & target[i].mark) != 0)
			{
				return true;
			}
		}
		return false;
	}

	bool AtomicTypeMark::IsSame(std::span<AtomicTypeMark const> source, std::span<AtomicTypeMark const> target)
	{
		assert(source.size() == target.size());
		for (std::size_t i = 0; i < target.size(); ++i)
		{
			if (source[i].mark != target[i].mark)
			{
				return false;
			}
		}
		return true;
	}

	auto Archetype::Create(AtomicTypeManager& manager, std::span<AtomicType::Ptr const> atomic_type, std::pmr::memory_resource* resource)
	->Ptr
	{
		auto storage_count = manager.GetStorageCount();
		auto tol_layout = Potato::MemLayout::MemLayoutCPP::Get<Archetype>();
		auto index_offset = tol_layout.Insert(Potato::IR::Layout::GetArray<AtomicTypeMark>(storage_count));
		auto offset = tol_layout.Insert(Potato::IR::Layout::GetArray<MemberView>(atomic_type.size()));

		auto layout = tol_layout.Get();
		
		auto re = Potato::IR::MemoryResourceRecord::Allocate(resource, layout);
		if(re)
		{
			std::span<MemberView> MV = std::span(reinterpret_cast<MemberView*>(re.GetByte() + offset), atomic_type.size());
			std::span<AtomicTypeMark> archetype_index{
				new (re.GetByte(index_offset)) AtomicTypeMark[storage_count],
				storage_count
			};
			;
			Potato::MemLayout::MemLayoutCPP total_layout;
			for (std::size_t i = 0; i < atomic_type.size(); ++i)
			{
				auto& ref = atomic_type[i];
				assert(ref);
				auto ope = ref->GetOperateProperty();
				
				std::size_t offset = total_layout.Insert(ref->GetLayout());
				auto inf = manager.LocateOrAddAtomicType(ref);

				if (!inf.has_value() || (!ope.default_construct && !ope.move_construct))
				{
					assert(false);
					for (std::size_t i2 = 0; i2 < i; ++i2)
					{
						MV[i].~MemberView();
					}
					re.Deallocate();
					return {};
				}

				new (&MV[i]) MemberView{
					ref,
					*inf,
					offset,
				};

				AtomicTypeMark::Mark(archetype_index, *inf);
			}
			auto archetype_layout = total_layout.GetRawLayout();
			return new(re.Get()) Archetype{
				re,  total_layout, MV, archetype_index
			};
		}
		return {};
	}

	std::optional<Archetype::Index> Archetype::LocateAtomicTypeID(AtomicTypeID id) const
	{
		for(std::size_t i =0; i < member_view.size(); ++i)
		{
			if(member_view[i].atomic_type_id == id)
				return Index{i};
		}
		return std::nullopt;
	}

	AtomicType::Ptr Archetype::GetTypeID(std::size_t index) const
	{
		if(member_view.size() > index)
		{
			return member_view[index].layout;
		}
		return {};
	}

	void* Archetype::Get(RawArray raw_data, std::size_t array_index)
	{
		assert(raw_data.array_count > array_index);
		return static_cast<std::byte*>(raw_data.buffer) + array_index * raw_data.element_layout.size;
	}

	Archetype::RawArray Archetype::Get(MemberView const& ref, ArrayMountPoint mount_point)
	{
		return RawArray{
			static_cast<std::byte*>(mount_point.GetBuffer()) + ref.offset * mount_point.total_count,
			mount_point.available_count,
			ref.layout->GetLayout()
		};
	}

	Archetype::~Archetype()
	{
		AtomicTypeMark::Destruction(archetype_mark);
		for(auto& ite : member_view)
		{
			ite.~MemberView();
		}
	}

}