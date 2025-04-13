module;

export module NoodlesBitFlag;

import std;
import Potato;
export import NoodlesMisc;

export namespace Noodles
{
	struct BitFlag
	{
		std::size_t value = 0;
		std::strong_ordering operator <=>(BitFlag const&) const = default;
		bool operator ==(BitFlag const&) const = default;
	};

	export struct BitFlagContainer;

	struct BitFlagConstContainer
	{
		using Element = std::size_t;

		BitFlagConstContainer() = default;
		BitFlagConstContainer(std::span<Element const> container) : container(container) {}
		BitFlagConstContainer(BitFlagConstContainer const&) = default;
		std::optional<bool> GetValue(BitFlag flag) const;
		std::optional<bool> Inclusion(BitFlagConstContainer target) const;
		std::optional<bool> IsOverlapping(BitFlagConstContainer target) const;
		std::optional<bool> IsOverlappingWithMask(BitFlagConstContainer target, BitFlagConstContainer mask) const;
		std::optional<bool> IsSame(BitFlagConstContainer target) const;
		std::size_t GetBitFlagCount() const;
		bool IsReset() const;

		static std::size_t GetBitFlagContainerElementCount(std::size_t max_bit_flag);
		static BitFlag GetMaxBitFlag(std::size_t max_bit_flag);

	protected:

		std::span<Element const> container;

		friend struct BitFlagContainer;
	};

	export struct BitFlagContainer : BitFlagConstContainer
	{
		BitFlagContainer() = default;
		BitFlagContainer(std::span<Element> container) : BitFlagConstContainer(container) {}
		BitFlagContainer(BitFlagContainer const&) = default;
		std::optional<bool> SetValue(BitFlag flag, bool value = true);
		void Reset();
		bool CopyFrom(BitFlagConstContainer source);
		bool Union(BitFlagConstContainer source);
		operator BitFlagConstContainer() const { return {container}; }
	};

	struct StructLayoutBitFlagMapping
	{
		std::optional<BitFlag> Locate(StructLayout const& type);
		std::optional<BitFlag> LocateOrAdd(StructLayout const& type);

		StructLayoutBitFlagMapping(std::size_t min_bit_flag_count = 128, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
			: bit_flag_container_count(BitFlagContainer::GetBitFlagContainerElementCount(min_bit_flag_count)),
			max_bit_flag(BitFlagContainer::GetMaxBitFlag(min_bit_flag_count)),
			struct_layouts(resource)
		{
		}

		std::size_t GetBitFlagContainerElementCount() const { return bit_flag_container_count; }
		BitFlag GetMaxBitFlag() const { return max_bit_flag; }

	protected:

		std::size_t const bit_flag_container_count = 0;
		BitFlag const max_bit_flag;
		std::pmr::vector<StructLayout::Ptr> struct_layouts;
	};
}