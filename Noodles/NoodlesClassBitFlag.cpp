module;

module NoodlesClassBitFlag;

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


	std::size_t BitFlagConstContainer::GetBitFlagContainerElementCount(std::size_t max_big_flag)
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

	BitFlag BitFlagConstContainer::GetMaxBitFlag(std::size_t max_bit_flag)
	{
		constexpr std::size_t size = sizeof(Element) * 8;
		std::size_t storage = GetBitFlagContainerElementCount(max_bit_flag);
		return BitFlag{ storage * size };
	}

	std::optional<bool> BitFlagConstContainer::GetValue(BitFlag flag) const
	{
		auto [mindex, moffset] = LocatedBitFlag(flag);
		if (mindex < container.size())
		{
			auto mark_value = (std::size_t{ 1 } << moffset);
			return (container[mindex] & mark_value) == mark_value;
		}
		return std::nullopt;
	}

	std::optional<bool> BitFlagConstContainer::Inclusion(BitFlagConstContainer target) const
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

	std::optional<bool> BitFlagConstContainer::IsOverlapping(BitFlagConstContainer target) const
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

	std::optional<bool> BitFlagConstContainer::IsOverlappingWithMask(BitFlagConstContainer target, BitFlagConstContainer mask) const
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

	std::optional<bool> BitFlagConstContainer::IsSame(BitFlagConstContainer target) const
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

	bool BitFlagConstContainer::IsReset() const
	{
		for (auto ite : container)
		{
			if (ite != 0)
				return false;
		}
		return true;
	}

	std::optional<bool> BitFlagContainer::SetValue(BitFlag flag, bool value)
	{
		auto [e_index, b_index] = LocatedBitFlag(flag);
		if (e_index < container.size())
		{
			std::span<Element> target = {const_cast<Element*>(container.data()), container.size()};
			auto bit_value = (BitFlagContainer::Element{ 1 } << b_index);
			auto old_value = container[e_index];
			if (value)
				target[e_index] |= bit_value;
			else
				target[e_index] &= (~bit_value);
			return (old_value & bit_value) == bit_value;
		}
		return std::nullopt;
	}

	void BitFlagContainer::Reset()
	{
		std::span<Element> target = { const_cast<Element*>(container.data()), container.size() };
		for (auto& ite : target)
		{
			ite = 0;
		}
	}

	bool BitFlagContainer::CopyFrom(BitFlagConstContainer source)
	{
		if (container.size() == source.container.size())
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

	bool BitFlagContainer::Union(BitFlagConstContainer source)
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

	std::optional<BitFlag> StructLayoutBitFlagMapping::Locate(StructLayout const& type)
	{
		for (std::size_t i = 0; i < struct_layouts.size(); ++i)
		{
			auto& ref = struct_layouts[i];
			if (*ref == type)
			{
				return BitFlag{ i };
			}
		}
		return std::nullopt;
	}
	std::optional<BitFlag> StructLayoutBitFlagMapping::LocateOrAdd(StructLayout const& type)
	{
		auto result = Locate(type);
		if (result.has_value())
		{
			return result;
		}
		else {
			auto current_size = struct_layouts.size();
			if (current_size + 1 < max_bit_flag.value)
			{
				struct_layouts.emplace_back(&type);
				return BitFlag{ current_size };
			}
		}
		return std::nullopt;
	}
}