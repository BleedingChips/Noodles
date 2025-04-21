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

	struct BitFlagContainerConstViewer
	{
		using Element = std::size_t;

		BitFlagContainerConstViewer() = default;
		BitFlagContainerConstViewer(std::span<Element const> container) : container(container) {}
		BitFlagContainerConstViewer(BitFlagContainerConstViewer const&) = default;
		std::optional<bool> GetValue(BitFlag flag) const;
		std::optional<bool> Inclusion(BitFlagContainerConstViewer target) const;
		std::optional<bool> IsOverlapping(BitFlagContainerConstViewer target) const;
		std::optional<bool> IsOverlappingWithMask(BitFlagContainerConstViewer target, BitFlagContainerConstViewer mask) const;
		std::optional<bool> IsSame(BitFlagContainerConstViewer target) const;
		std::size_t GetBitFlagCount() const;
		bool IsReset() const;

		static std::size_t GetBitFlagContainerElementCount(std::size_t max_bit_flag);
		static BitFlag GetMaxBitFlag(std::size_t max_bit_flag);
		std::span<Element const> AsSpan() const { return container; };

	protected:

		std::span<Element const> container;

		friend struct BitFlagContainerViewer;
	};

	export struct BitFlagContainerViewer : BitFlagContainerConstViewer
	{
		BitFlagContainerViewer() = default;
		BitFlagContainerViewer(std::span<Element> container) : BitFlagContainerConstViewer(container) {}
		BitFlagContainerViewer(BitFlagContainerViewer const&) = default;
		std::optional<bool> SetValue(BitFlag flag, bool value = true);
		void Reset();
		bool CopyFrom(BitFlagContainerConstViewer source);
		bool Union(BitFlagContainerConstViewer source);
		bool ExclusiveOr(BitFlagContainerConstViewer source1, BitFlagContainerConstViewer source2);
		operator BitFlagContainerConstViewer() const { return {container}; }
	};

	struct BitFlagContainer
	{
		using Element = BitFlagContainerViewer::Element;
		BitFlagContainer(std::size_t container_count, std::pmr::memory_resource* resource = std::pmr::get_default_resource());
		BitFlagContainer(BitFlagContainer&&) = default;
		operator BitFlagContainerConstViewer() const { return BitFlagContainerConstViewer{ std::span(container) }; }
		operator BitFlagContainerViewer() { return BitFlagContainerViewer{ std::span(container) }; }
		std::span<Element> AsSpan() { return std::span(container); }
	protected:
		std::pmr::vector<Element> container;
	};
}