module;

module NoodlesClassBitFlag;

namespace Noodles
{

	std::optional<BitFlag> ClassBitFlagMap::Locate(StructLayout const& type) const
	{
		for (std::size_t index = 0; index < struct_layouts.size(); ++index)
		{
			auto& tar = struct_layouts[index];
			if (*tar == type)
			{
				return BitFlag{index};
			}
		}
		return std::nullopt;
	}

	std::optional<BitFlag> ClassBitFlagMap::LocateOrAdd(StructLayout const& type)
	{
		auto result = Locate(type);
		if (result.has_value())
		{
			return result;
		}
		else if(struct_layouts.size() < max_bit_flag.value){
			BitFlag new_result{ struct_layouts.size()};
			struct_layouts.emplace_back(&type);
			return new_result;
		}
		return result;
	}

	std::optional<BitFlag> Locate(StructLayout const& type);



	std::optional<BitFlag> AsynClassBitFlagMap::Locate(StructLayout const& struct_layout) const
	{
		std::shared_lock sl(mutex);
		return ClassBitFlagMap::Locate(struct_layout);
	}

	std::optional<BitFlag> AsynClassBitFlagMap::LocateOrAdd(StructLayout const& struct_layout)
	{
		{
			std::shared_lock sl(mutex);
			auto re = ClassBitFlagMap::Locate(struct_layout);
			if (re.has_value())
			{
				return re;
			}
		}

		std::lock_guard lg(mutex);
		return ClassBitFlagMap::LocateOrAdd(struct_layout);
	}
	
	/*
	GlobalContext::GlobalContext(Potato::IR::MemoryResourceRecord record, Config config)
		: MemoryResourceRecordIntrusiveInterface(record),
		component_bigflag_map(config.max_component_class_count, record.GetMemoryResource()),
		singleton_bigflag_map(config.max_singleton_class_count, record.GetMemoryResource()),
		threadorder_bigflag_map(config.max_thread_order_class_count, record.GetMemoryResource()),
		component_query_map(component_bigflag_map.GetBitFlagContainerElementCount(), record.GetMemoryResource())
	{

	}

	

	GlobalContext::Ptr GlobalContext::Create(Config config, std::pmr::memory_resource* resource)
	{
		auto re = Potato::IR::MemoryResourceRecord::Allocate<GlobalContext>(resource);
		if (re)
		{
			return new(re.Get()) GlobalContext{re, config};
		}
		return {};
	}

	

	OptionalSizeT GlobalContext::GetQueryIndex(BitFlagConstContainer target, std::shared_mutex& mutex, BitFlagContainerContainer& mapping)
	{
		{
			std::shared_lock sl(mutex);
			auto result = mapping.Find(target);
			if (result)
			{
				return result;
			}
		}

		{
			std::lock_guard lg(mutex);
			return mapping.FindOrCreate(target);
		}
	}
	*/
}