module;

module NoodlesGlobalContext;

namespace Noodles
{
	GlobalContext::GlobalContext(Potato::IR::MemoryResourceRecord record, Config config)
		: MemoryResourceRecordIntrusiveInterface(record),
		component_bigflag_map(config.max_component_class_count, record.GetMemoryResource()),
		singleton_bigflag_map(config.max_singleton_class_count, record.GetMemoryResource()),
		threadorder_bigflag_map(config.max_thread_order_class_count, record.GetMemoryResource())
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

	std::optional<BitFlag> GlobalContext::LocateBitFlag(StructLayout const& struct_layout, std::shared_mutex& mutex, StructLayoutBitFlagMapping& mapping)
	{
		{
			std::shared_lock sl(mutex);
			auto result = mapping.Locate(struct_layout);
			if (result.has_value())
			{
				return result;
			}
		}

		{
			std::lock_guard lg(mutex);
			return mapping.LocateOrAdd(struct_layout);
		}
	}

}