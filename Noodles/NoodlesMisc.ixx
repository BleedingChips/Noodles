module;

#include <cassert>
export module NoodlesMisc;

import std;
import Potato;

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

	struct StructLayoutMarksInfosView
	{
		std::span<MarkElement const> write_marks;
		std::span<MarkElement const> total_marks;
		operator bool() const { return !write_marks.empty(); }

		bool WriteConfig(StructLayoutMarksInfosView const& other) const
		{
			return MarkElement::IsOverlapping(
				write_marks, other.total_marks
			) || MarkElement::IsOverlapping(
				other.total_marks, write_marks
			);
		}

		bool WriteConfigWithMask(StructLayoutMarksInfosView const& other, std::span<MarkElement const> mask) const
		{
			return MarkElement::IsOverlappingWithMask(
				write_marks, other.total_marks, mask
			) || MarkElement::IsOverlappingWithMask(
				other.total_marks, write_marks, mask
			);
		}
	};

	struct StructLayoutMarksInfos
	{
		std::span<MarkElement> write_marks;
		std::span<MarkElement> total_marks;
		operator StructLayoutMarksInfosView() const { return { write_marks, total_marks }; }
		void MarkFrom(StructLayoutMarksInfosView target);
		operator bool() const { return !write_marks.empty(); }
	};

	template<typename Type>
	concept HasRemoveComponentWriteProperty = requires(Type type)
	{
		{ Type::RemoveComponentWriteProperty }->std::same_as<std::true_type>;
	};

	template<typename Type>
	concept HasRemoveSingletonWriteProperty = requires(Type type)
	{
		{ Type::RemoveSingletonWriteProperty }->std::same_as<std::true_type>;
	};

	template<typename Type>
	concept IsFilterWriteType = std::is_same_v<Type, std::remove_cvref_t<Type>>;

	template<typename Type>
	concept IsFilterReadType = std::is_same_v<Type, std::add_const_t<std::remove_cvref_t<Type>>>;

	template<typename Type>
	concept AcceptableFilterType = IsFilterWriteType<Type> || IsFilterReadType<Type>;

	struct StructLayoutWriteProperty
	{
		bool need_write = false;
		StructLayout::Ptr struct_layout;
		template<AcceptableFilterType Type>
		static StructLayoutWriteProperty GetComponent()
		{
			if constexpr (HasRemoveComponentWriteProperty<Type>)
			{
				return {false, StructLayout::GetStatic<Type>()};
			}else
			{
				return { IsFilterWriteType<Type>,StructLayout::GetStatic<Type>() };
			}
		}
		template<AcceptableFilterType Type>
		static StructLayoutWriteProperty GetSingleton()
		{
			if constexpr (HasRemoveSingletonWriteProperty<Type>)
			{
				return { false, StructLayout::GetStatic<Type>() };
			}
			else
			{
				return { IsFilterWriteType<Type>,StructLayout::GetStatic<Type>() };
			}
		}
		template<AcceptableFilterType Type>
		static StructLayoutWriteProperty Get()
		{
			return { IsFilterWriteType<Type>,StructLayout::GetStatic<Type>() };
		}
	};

	struct StructLayoutMarkIndexManager
	{
		std::optional<MarkIndex> LocateOrAdd(StructLayout const& type);
		StructLayoutMarkIndexManager(std::size_t struct_layout_max_count = 128, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
			: storage_count(MarkElement::GetMarkElementStorageCalculate(struct_layout_max_count)),
			max_count(MarkElement::GetMaxMarkIndexCount(struct_layout_max_count)),
			struct_layouts(resource)
		{
		}
		std::size_t GetStorageCount() const { return storage_count; }
		std::size_t GetMaxStructLayoutCount() const { return max_count; }
	protected:
		std::optional<MarkIndex> Locate_AssumedLocked(StructLayout const& type) const;
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