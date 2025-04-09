module;

#include <cassert>

export module NoodlesMisc;

import std;
import Potato;

export namespace Noodles
{
	using StructLayout = Potato::IR::StructLayout;

	

	
	/*
	struct ReadWriteBitFlagContainerView
	{
		std::span<BitFlagContainer const> write_marks;
		std::span<BitFlagContainer const> total_marks;
		operator bool() const { return !write_marks.empty(); }

		bool WriteOverlapping(ReadWriteBitFlagContainerView const& other) const
		{
			return BitFlagContainer::IsOverlapping(
				write_marks, other.total_marks
			) || BitFlagContainer::IsOverlapping(
				other.total_marks, write_marks
			);
		}

		bool WriteOverlappingWithMask(ReadWriteBitFlagContainerView const& other, std::span<BitFlagContainer const> mask) const
		{
			return BitFlagContainer::IsOverlappingWithMask(
				write_marks, other.total_marks, mask
			) || BitFlagContainer::IsOverlappingWithMask(
				other.total_marks, write_marks, mask
			);
		}
	};

	template<typename Type>
	concept HasRemoveComponentWriteProperty = requires(Type)
	{
		std::is_same_v<typename Type::RemoveComponentWriteProperty, std::true_type>;
	};

	template<typename Type>
	concept HasRemoveSingletonWriteProperty = requires(Type)
	{
		std::is_same_v<typename Type::RemoveSingletonWriteProperty, std::true_type>;
	};

	template<typename Type>
	concept IsQueryWriteType = std::is_same_v<Type, std::remove_cvref_t<Type>>;

	template<typename Type>
	concept IsQueryReadType = std::is_same_v<Type, std::add_const_t<std::remove_cvref_t<Type>>>;

	template<typename Type>
	concept AcceptableQueryType = IsQueryWriteType<Type> || IsQueryReadType<Type>;

	struct StructLayoutWriteProperty
	{
		bool need_write = false;
		StructLayout::Ptr struct_layout;
		template<AcceptableQueryType Type>
		static StructLayoutWriteProperty GetComponent()
		{
			if constexpr (HasRemoveComponentWriteProperty<Type>)
			{
				return {false, StructLayout::GetStatic<Type>()};
			}else
			{
				return { IsQueryWriteType<Type>,StructLayout::GetStatic<Type>() };
			}
		}
		template<AcceptableQueryType Type>
		static StructLayoutWriteProperty GetSingleton()
		{
			if constexpr (HasRemoveSingletonWriteProperty<Type>)
			{
				return { false, StructLayout::GetStatic<Type>() };
			}
			else
			{
				return { IsQueryWriteType<Type>,StructLayout::GetStatic<Type>() };
			}
		}
		template<AcceptableQueryType Type>
		static StructLayoutWriteProperty Get()
		{
			return { IsQueryWriteType<Type>,StructLayout::GetStatic<Type>() };
		}
	};

	
	*/

	struct OptionalSizeT
	{
		std::size_t real_index = std::numeric_limits<std::size_t>::max();
		operator bool() const { return real_index != std::numeric_limits<std::size_t>::max(); }
		operator std::size_t() const { assert(*this); return real_index; }
		OptionalSizeT& operator=(OptionalSizeT const&) = default;
		OptionalSizeT& operator=(std::size_t input) { assert(input != std::numeric_limits<std::size_t>::max()); real_index = input; return *this; }
		OptionalSizeT() = default;
		OptionalSizeT(OptionalSizeT const&) = default;
		OptionalSizeT(std::size_t input) : real_index(input) {}
		void Reset() { real_index = std::numeric_limits<std::size_t>::max(); }
		std::size_t Get() const { return real_index; }
	};
}