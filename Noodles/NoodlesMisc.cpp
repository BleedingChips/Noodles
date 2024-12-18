module;

#include <cassert>

module NoodlesMisc;

import PotatoMisc;
import PotatoMemLayout;

namespace Noodles
{

	std::tuple<std::size_t, std::size_t> Locate(MarkIndex index)
	{
		constexpr std::size_t size = sizeof(MarkElement::mark) * 8;
		return {
			index.index / size,
			index.index % size,
		};
	}


	std::size_t MarkElement::GetMarkElementStorageCalculate(std::size_t mark_index_count)
	{
		constexpr std::size_t size = sizeof(MarkElement::mark) * 8;
		std::size_t i = mark_index_count / size;
		std::size_t i2 = mark_index_count % size;
		if(i2 == 0)
		{
			return i;
		}else
		{
			return i + 1;
		}
	}
	std::size_t MarkElement::GetMaxMarkIndexCount(std::size_t mark_index_count)
	{
		constexpr std::size_t size = sizeof(MarkElement::mark) * 8;
		std::size_t storage = GetMarkElementStorageCalculate(mark_index_count);
		return storage * size;
	}

	bool MarkElement::Mark(std::span<MarkElement> marks, MarkIndex index, bool mark)
	{
		auto [mindex, moffset] = Locate(index);
		assert(mindex < marks.size());
		auto mark_value = (std::size_t{ 1 } << moffset);
		auto old_value = marks[mindex].mark;
		if (mark)
			marks[mindex].mark |= mark_value;
		else
			marks[mindex].mark &= (~mark_value);
		return (old_value & mark_value) == mark_value;
	}

	bool MarkElement::CheckIsMark(std::span<MarkElement const> marks, MarkIndex index)
	{
		auto [mindex, moffset] = Locate(index);
		assert(mindex < marks.size());
		auto mark_value = (std::size_t{ 1 } << moffset);
		return (marks[mindex].mark & mark_value) == mark_value;
	}

	bool MarkElement::Inclusion(std::span<MarkElement const> source, std::span<MarkElement const> target)
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

	bool MarkElement::IsOverlapping(std::span<MarkElement const> source, std::span<MarkElement const> target)
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

	bool MarkElement::IsOverlappingWithMask(std::span<MarkElement const> source, std::span<MarkElement const> target, std::span<MarkElement const> mask)
	{
		assert(source.size() == target.size() && source.size() == mask.size());
		for (std::size_t i = 0; i < source.size(); ++i)
		{
			if ((source[i].mark & target[i].mark & mask[i].mark) != 0)
			{
				return true;
			}
		}
		return false;
	}

	bool MarkElement::IsSame(std::span<MarkElement const> source, std::span<MarkElement const> target)
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

	void MarkElement::Reset(std::span<MarkElement> target)
	{
		for (auto& ite : target)
		{
			ite.mark = 0;
		}
	}

	void MarkElement::CopyTo(std::span<MarkElement const> source, std::span<MarkElement> target)
	{
		assert(source.size() == target.size());
		for (std::size_t i = 0; i < source.size(); ++i)
		{
			target[i] = source[i];
		}
	}

	void MarkElement::MarkTo(std::span<MarkElement const> source, std::span<MarkElement> target)
	{
		assert(source.size() == target.size());
		for (std::size_t i = 0; i < source.size(); ++i)
		{
			target[i].mark |= source[i].mark;
		}
	}

	bool MarkElement::IsReset(std::span<MarkElement const> target)
	{
		for (auto& ite : target)
		{
			if (ite.mark != 0)
				return false;
		}
		return true;
	}

	void StructLayoutMarksInfos::MarkFrom(StructLayoutMarksInfosView target)
	{
		MarkElement::MarkTo(target.total_marks, total_marks);
		MarkElement::MarkTo(target.write_marks, write_marks);
	}

	std::optional<MarkIndex> StructLayoutMarkIndexManager::Locate_AssumedLocked(StructLayout const& type) const
	{
		for (std::size_t i = 0; i < struct_layouts.size(); ++i)
		{
			auto& ref = struct_layouts[i];
			if (*ref == type)
			{
				return MarkIndex{ i };
			}
		}
		return std::nullopt;
	}

	std::optional<MarkIndex> StructLayoutMarkIndexManager::LocateOrAdd(StructLayout const& type)
	{
		{
			std::shared_lock sl(mutex);
			auto re = Locate_AssumedLocked(type);
			if (re.has_value())
			{
				return re;
			}
		}

		{
			std::lock_guard lg(mutex);
			auto re = Locate_AssumedLocked(type);
			if (re.has_value())
			{
				return re;
			}
			auto count = struct_layouts.size();
			if(count < GetMaxStructLayoutCount())
			{
				struct_layouts.emplace_back(&type);
				return MarkIndex{count};
			}
		}
		return std::nullopt;
	}

	
}