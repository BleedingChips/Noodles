module;

export module NoodlesGlobalContext;

import std;
import Potato;
import NoodlesClassBitFlag;

export namespace Noodles
{
	struct GlobalContext : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		struct Config
		{
			std::size_t max_component_class_count = 128;
			std::size_t max_singleton_class_count = 128;
			std::size_t max_thread_order_class_count = 128;
			std::size_t max_component_filter_count = 128;
			std::size_t max_singleton_filter_count = 128;
		};

		using Ptr = Potato::Pointer::IntrusivePtr<GlobalContext>;

		static Ptr Create(Config config = {}, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

	protected:

		GlobalContext(Potato::IR::MemoryResourceRecord record, Config config);

		mutable std::shared_mutex component_bigflag_map_mutex;
		StructLayoutBitFlagMapping component_bigflag_map;

		mutable std::shared_mutex singleton_bigflag_map_mutex;
		StructLayoutBitFlagMapping singleton_bigflag_map;

		mutable std::shared_mutex threadorder_bigflag_map_mutex;
		StructLayoutBitFlagMapping threadorder_bigflag_map;

		friend struct Ptr::CurrentWrapper;

	};
}