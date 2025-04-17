module;

export module NoodlesGlobalContext;

import std;
import Potato;
import NoodlesBitFlag;

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

		std::size_t GetComponentBitFlagContainerElementCount() const { return component_bigflag_map.GetBitFlagContainerElementCount(); }
		std::optional<BitFlag> GetComponentBitFlag(StructLayout const& struct_layout) { return LocateBitFlag(struct_layout, component_bigflag_map_mutex, component_bigflag_map); }
		
		std::size_t GetComponentQueryIndex(std::span<BitFlag const> query_class);

		template<typename ...ComponentClass>
		std::size_t GetComponentQueryIndex()
		{
			static std::array<BitFlag, sizeof...(ComponentClass)> temp = {
				*this->GetComponentBitFlag(
					*Potato::IR::StaticAtomicStructLayout<ComponentClass>::Create()
				)...
			};
			return GetComponentQueryIndex(temp);
		}

	protected:

		GlobalContext(Potato::IR::MemoryResourceRecord record, Config config);

		static std::optional<BitFlag> LocateBitFlag(StructLayout const& struct_layout, std::shared_mutex& mutex, StructLayoutBitFlagMapping& mapping);


		mutable std::shared_mutex component_bigflag_map_mutex;
		StructLayoutBitFlagMapping component_bigflag_map;

		mutable std::shared_mutex singleton_bigflag_map_mutex;
		StructLayoutBitFlagMapping singleton_bigflag_map;

		mutable std::shared_mutex threadorder_bigflag_map_mutex;
		StructLayoutBitFlagMapping threadorder_bigflag_map;

		mutable std::shared_mutex component_query_mutex;
		struct ComponentQueryInifo
		{
			BitFlagConstContainer container;
			Potato::Misc::IndexSpan<> component_class_span;
		};

		std::pmr::vector<BitFlag> component_class;
		std::pmr::vector<BitFlagConstContainer::Element> component_bitflag;

		friend struct Ptr::CurrentWrapper;

	};
}