module;

#include <cassert>
export module NoodlesMisc;

import std;
import PotatoIR;
import PotatoPointer;
import PotatoTaskSystem;

export namespace Noodles
{
	struct AtomicTypeMark;

	using StructLayout = Potato::IR::StructLayout;

	struct MarkIndex
	{
		std::size_t index = 0;
		std::strong_ordering operator <=>(MarkIndex const&) const = default;
		bool operator ==(MarkIndex const&) const = default;
	};

	struct MarkElement
	{
		std::size_t mark = 0;

		static bool Mark(std::span<MarkElement> marks, MarkIndex index, bool mark = true);
		static bool CheckIsMark(std::span<MarkElement const> marks, MarkIndex index);
		static bool Inclusion(std::span<MarkElement const> source, std::span<MarkElement const> target);
		static bool IsOverlapping(std::span<MarkElement const> source, std::span<MarkElement const> target);
		static bool IsOverlappingWithMask(std::span<MarkElement const> source, std::span<MarkElement const> target, std::span<MarkElement const> mask);
		static void Reset(std::span<MarkElement> target);
		static bool IsReset(std::span<MarkElement const> target);
		static bool IsSame(std::span<MarkElement const> source, std::span<MarkElement const> target);
		static void CopyTo(std::span<MarkElement const> source, std::span<MarkElement> target);
		static void MarkTo(std::span<MarkElement const> source, std::span<MarkElement> target);
		static std::size_t GetMarkElementStorageCalculate(std::size_t mark_index_count);
		static std::size_t GetMaxMarkIndexCount(std::size_t mark_index_count);
	};

	struct WrittenMarkElementSpan
	{
		std::span<MarkElement const> write_marks;
		std::span<MarkElement const> total_marks;
		operator bool() const { return !write_marks.empty(); }

		bool WriteConfig(WrittenMarkElementSpan const& other) const
		{
			return MarkElement::IsOverlapping(
				write_marks, other.total_marks
			) || MarkElement::IsOverlapping(
				other.total_marks, write_marks
			);
		}

		bool WriteConfigWithMask(WrittenMarkElementSpan const& other, std::span<MarkElement const> mask) const
		{
			return MarkElement::IsOverlappingWithMask(
				write_marks, other.total_marks, mask
			) || MarkElement::IsOverlappingWithMask(
				other.total_marks, write_marks, mask
			);
		}
	};

	struct WrittenMarkElementSpanWriteable
	{
		std::span<MarkElement> write_marks;
		std::span<MarkElement> total_marks;
		operator WrittenMarkElementSpan() const { return { write_marks, total_marks }; }
		void MarkFrom(WrittenMarkElementSpan target);
		operator bool() const { return !write_marks.empty(); }
	};
	

	struct StructLayoutMarkIndexManager
	{
		std::optional<MarkIndex> LocateOrAdd(StructLayout::Ptr const& type);
		StructLayoutMarkIndexManager(std::size_t struct_layout_max_count = 128, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
			: storage_count(MarkElement::GetMarkElementStorageCalculate(struct_layout_max_count)),
			max_count(MarkElement::GetMaxMarkIndexCount(struct_layout_max_count)),
			struct_layouts(resource)
		{
		}
		std::size_t GetStorageCount() const { return storage_count; }
		std::size_t GetMaxStructLayoutCount() const { return max_count; }
	protected:
		std::optional<MarkIndex> Locate_AssumedLocked(StructLayout::Ptr const& type) const;
		std::size_t const storage_count = 0;
		std::size_t const max_count = 0;
		std::shared_mutex mutex;
		std::pmr::vector<StructLayout::Ptr> struct_layouts;
	};

	struct OptionalIndex
	{
		std::size_t real_index = std::numeric_limits<std::size_t>::max();
		operator bool() const { return real_index != std::numeric_limits<std::size_t>::max(); }
		operator std::size_t() const { assert(*this); return real_index; }
		OptionalIndex& operator=(OptionalIndex const&) = default;
		OptionalIndex& operator=(std::size_t input) { assert(input != std::numeric_limits<std::size_t>::max()); real_index = input; return *this; }
		OptionalIndex() {};
		OptionalIndex(OptionalIndex const&) = default;
		OptionalIndex(std::size_t input) : real_index(input) {}
		void Reset() { real_index = std::numeric_limits<std::size_t>::max(); }
		std::size_t Get() const { return real_index; }
	};

}