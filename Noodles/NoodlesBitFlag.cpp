module;

module NoodlesBitFlag;

namespace Noodles
{
	std::tuple<std::size_t, std::size_t> LocatedBitFlag(BitFlag flag)
	{
		constexpr std::size_t size = sizeof(BitFlagContainer::Element) * 8;
		return {
			flag.value / size,
			flag.value % size,
		};
	}


	std::size_t BitFlagContainerConstViewer::GetBitFlagContainerElementCount(std::size_t max_big_flag)
	{
		constexpr std::size_t size = sizeof(Element) * 8;

		std::size_t i = max_big_flag / size;
		std::size_t i2 = max_big_flag % size;
		if (i2 == 0)
		{
			return i;
		}
		else
		{
			return i + 1;
		}
	}

	BitFlag BitFlagContainerConstViewer::GetMaxBitFlag(std::size_t max_bit_flag)
	{
		constexpr std::size_t size = sizeof(Element) * 8;
		std::size_t storage = GetBitFlagContainerElementCount(max_bit_flag);
		return BitFlag{ storage * size };
	}

	std::optional<bool> BitFlagContainerConstViewer::GetValue(BitFlag flag) const
	{
		auto [mindex, moffset] = LocatedBitFlag(flag);
		if (mindex < container.size())
		{
			auto mark_value = (std::size_t{ 1 } << moffset);
			return (container[mindex] & mark_value) == mark_value;
		}
		return std::nullopt;
	}

	std::optional<bool> BitFlagContainerConstViewer::Inclusion(BitFlagContainerConstViewer target) const
	{
		if (container.size() == target.container.size())
		{
			for (std::size_t i = 0; i < target.container.size(); ++i)
			{
				if ((container[i] & target.container[i]) != target.container[i])
				{
					return false;
				}
			}
			return true;
		}
		return std::nullopt;
	}

	std::optional<bool> BitFlagContainerConstViewer::IsOverlapping(BitFlagContainerConstViewer target) const
	{
		if (container.size() == target.container.size())
		{
			for (std::size_t i = 0; i < target.container.size(); ++i)
			{
				if ((container[i] & target.container[i]) != 0)
				{
					return true;
				}
			}
			return false;
		}
		return std::nullopt;
	}

	std::optional<bool> BitFlagContainerConstViewer::IsOverlappingWithMask(BitFlagContainerConstViewer target, BitFlagContainerConstViewer mask) const
	{
		if (container.size() == target.container.size() && container.size() == mask.container.size())
		{
			for (std::size_t i = 0; i < container.size(); ++i)
			{
				if ((container[i] & target.container[i] & mask.container[i]) != 0)
				{
					return true;
				}
			}
			return false;
		}
		return std::nullopt;
	}

	std::optional<bool> BitFlagContainerConstViewer::IsSame(BitFlagContainerConstViewer target) const
	{
		if (container.size() == target.container.size())
		{
			for (std::size_t i = 0; i < container.size(); ++i)
			{
				if (container[i] != target.container[i])
				{
					return false;
				}
			}
			return true;
		}
		return std::nullopt;
	}

	bool BitFlagContainerConstViewer::IsReset() const
	{
		for (auto ite : container)
		{
			if (ite != 0)
				return false;
		}
		return true;
	}

	std::size_t BitFlagContainerConstViewer::GetBitFlagCount() const
	{
		static auto bitflag_count_map = std::array<std::size_t, 16>{
			0, // 0000
			1, // 0001
			1, // 0010
			2, // 0011
			1, // 0100
			2, // 0101
			2, // 0110
			3, // 0111
			1, // 1000
			2, // 1001
			2, // 1010
			3, // 1011
			2, // 1100
			3, // 1101
			3, // 1110
			4  // 1111
		};

		std::size_t count = 0;

		for (auto ite : container)
		{
			if (ite != 0)
			{
				std::span<std::uint8_t> detect_span = {reinterpret_cast<std::uint8_t*>(&ite), sizeof(Element)};
				for (auto ite : detect_span)
				{
					if (ite != 0)
					{
						count += bitflag_count_map[(ite % 16)];
						count += bitflag_count_map[(ite / 16)];
					}
				}
			}
		}

		return count;
	}

	std::optional<bool> BitFlagContainerViewer::SetValue(BitFlag flag, bool value)
	{
		auto [e_index, b_index] = LocatedBitFlag(flag);
		if (e_index < container.size())
		{
			std::span<Element> target = {const_cast<Element*>(container.data()), container.size()};
			auto bit_value = (BitFlagContainerViewer::Element{ 1 } << b_index);
			auto old_value = container[e_index];
			if (value)
				target[e_index] |= bit_value;
			else
				target[e_index] &= (~bit_value);
			return (old_value & bit_value) == bit_value;
		}
		return std::nullopt;
	}

	void BitFlagContainerViewer::Reset()
	{
		std::span<Element> target = { const_cast<Element*>(container.data()), container.size() };
		for (auto& ite : target)
		{
			ite = 0;
		}
	}

	bool BitFlagContainerViewer::CopyFrom(BitFlagContainerConstViewer source)
	{
		if (container.size() == source.GetBitFlagCount())
		{
			std::span<Element> target = { const_cast<Element*>(container.data()), container.size() };
			for (std::size_t i = 0; i < container.size(); ++i)
			{
				target[i] = source.container[i];
			}
			return true;
		}
		return false;
	}

	bool BitFlagContainerViewer::Union(BitFlagContainerConstViewer source)
	{
		if (container.size() == source.container.size())
		{
			std::span<Element> target = { const_cast<Element*>(container.data()), container.size() };
			for (std::size_t i = 0; i < container.size(); ++i)
			{
				target[i] |= source.container[i];
			}
			return true;
		}
		return false;
	}

	bool BitFlagContainerViewer::ExclusiveOr(BitFlagContainerConstViewer source1, BitFlagContainerConstViewer source2)
	{
		if (container.size() == source1.container.size() && container.size() == source2.container.size())
		{
			std::span<Element> target = { const_cast<Element*>(container.data()), container.size() };
			for (std::size_t i = 0; i < container.size(); ++i)
			{
				target[i] = source1.container[i] ^ source2.container[i];
			}
			return true;
		}
		return false;
	}

	BitFlagContainer::BitFlagContainer(std::size_t container_count, std::pmr::memory_resource* resource)
		: container(resource)
	{
		container.resize(container_count, 0);
	}
}