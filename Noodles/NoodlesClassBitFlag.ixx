module;

export module NoodlesClassBitFlag;

import std;
import Potato;
import NoodlesBitFlag;

export namespace Noodles
{

	struct ClassBitFlagMap
	{
		std::optional<BitFlag> Locate(StructLayout const& type) const;
		std::optional<BitFlag> LocateOrAdd(StructLayout const& type);

		ClassBitFlagMap(std::size_t min_bit_flag_count = 128, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
			: bit_flag_container_count(BitFlagContainerConstViewer::GetBitFlagContainerElementCount(min_bit_flag_count)),
			max_bit_flag(BitFlagContainerConstViewer::GetMaxBitFlag(min_bit_flag_count)),
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

	struct AsynClassBitFlagMap : protected ClassBitFlagMap
	{
		using ClassBitFlagMap::ClassBitFlagMap;

		template<typename T>
		std::optional<BitFlag> Locate() const { return Locate(*Potato::IR::StaticAtomicStructLayout<T>::Create()); }
		template<typename T>
		std::optional<BitFlag> LocateOrAdd() { return LocateOrAdd(*Potato::IR::StaticAtomicStructLayout<T>::Create()); }

		std::optional<BitFlag> Locate(StructLayout const& type) const;
		std::optional<BitFlag> LocateOrAdd(StructLayout const& type);
		std::size_t GetBitFlagContainerElementCount() const { return bit_flag_container_count; }
		BitFlag GetMaxBitFlag() const { return max_bit_flag; }
	protected:
		mutable std::shared_mutex mutex;
	};


	/*
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

		std::size_t GetComponentBitFlagContainerElementCount() const { return component_bigflag_map.GetBitFlagContainerElementCount(); }
		std::optional<BitFlag> GetComponentBitFlag(StructLayout const& struct_layout) { return LocateBitFlag(struct_layout, component_bigflag_map_mutex, component_bigflag_map); }
		
		std::size_t GetComponentQueryIdentity(BitFlagConstContainer component_class_bitflag) { return GetQueryIndex(component_class_bitflag, component_query_mutex, component_query_map); }

	protected:

		GlobalContext(Potato::IR::MemoryResourceRecord record, Config config);

		static std::optional<BitFlag> LocateBitFlag(StructLayout const& struct_layout, std::shared_mutex& mutex, StructLayoutBitFlagMapping& mapping);
		static OptionalSizeT GetQueryIndex(BitFlagConstContainer target, std::shared_mutex& mutex, BitFlagContainerContainer& mapping);

		mutable std::shared_mutex component_bigflag_map_mutex;
		StructLayoutBitFlagMapping component_bigflag_map;

		mutable std::shared_mutex singleton_bigflag_map_mutex;
		StructLayoutBitFlagMapping singleton_bigflag_map;

		mutable std::shared_mutex threadorder_bigflag_map_mutex;
		StructLayoutBitFlagMapping threadorder_bigflag_map;

		mutable std::shared_mutex component_query_mutex;
		BitFlagContainerContainer component_query_map;

		friend struct Ptr::CurrentWrapper;

	};
	*/
}