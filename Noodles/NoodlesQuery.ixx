module;

#include <cassert>

export module NoodlesQuery;

import std;
import Potato;

import NoodlesArchetype;
import NoodlesBitFlag;
import NoodlesComponent;
import NoodlesSingleton;

export namespace Noodles
{

	struct ComponentQuery : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<ComponentQuery>;

		BitFlagContainerConstViewer GetRequire() const { return require_component; }
		BitFlagContainerConstViewer GetRefuse() const { return refuse_component; }
		BitFlagContainerConstViewer GetUsage() const { return archetype_usable; }

		static Ptr Create(
			std::size_t archetype_container_count, 
			std::size_t component_container_count, 
			std::span<BitFlag const> require_component_bitflag,
			std::span<BitFlag const> require_write_component_bitflag,
			std::span<BitFlag const> refuse_component_bitflag,
			std::pmr::memory_resource* resource = std::pmr::get_default_resource()
		);

		bool UpdateQueryData(ComponentManager const& manager);

		std::size_t GetArchetypeCount() const { return archetype_count; }

		std::optional<std::size_t> QueryComponentArrayWithIterator(ComponentManager& manager, std::size_t iterator, std::size_t chunk_index, std::span<void*> output_component);
		bool QueryComponent(ComponentManager& manager, ComponentManager::Index component_index, std::span<void*> output_component);

		~ComponentQuery() = default;

	protected:

		ComponentQuery(
			Potato::IR::MemoryResourceRecord record,
			BitFlagContainerConstViewer require_component,
			BitFlagContainerConstViewer require_write_component,
			BitFlagContainerConstViewer refuse_component,
			BitFlagContainerViewer archetype_usable,
			std::span<BitFlag const> component_bitflag
		)
			:MemoryResourceRecordIntrusiveInterface(record), 
			require_component(require_component),
			require_write_component(require_write_component),
			refuse_component(refuse_component),
			archetype_usable(archetype_usable),
			component_bitflag(component_bitflag),
			query_data(record.GetMemoryResource())
		{
			query_data_fast_offset = component_bitflag.size() * ComponentManager::GetQueryDataCount() + 1;
		}


		BitFlagContainerConstViewer require_component;
		BitFlagContainerConstViewer require_write_component;
		BitFlagContainerConstViewer refuse_component;
		BitFlagContainerViewer archetype_usable;
		std::span<BitFlag const> component_bitflag;
		std::pmr::vector<std::size_t> query_data;
		std::size_t archetype_count = 0;
		std::size_t updated_archetype_count = 0;
		std::size_t query_data_fast_offset = 0;

		friend struct Ptr::CurrentWrapper;
	};

	struct SingletonQuery : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<SingletonQuery>;

		BitFlagContainerConstViewer GetUsage() const { return singleton_usable; }

		static Ptr Create(
			std::size_t singleton_container_count,
			std::span<BitFlag const> singleton_bitflag,
			std::pmr::memory_resource* resource = std::pmr::get_default_resource()
		);

		bool UpdateQueryData(SingletonManager const& manager);

		bool QuerySingleton(SingletonManager const& manager, std::span<void*> output_component);

		~SingletonQuery() = default;

	protected:

		SingletonQuery(
			Potato::IR::MemoryResourceRecord record,
			BitFlagContainerViewer singleton_usable,
			std::span<BitFlag const> singleton_bitflag,
			std::span<std::size_t> query_data
		)
			:MemoryResourceRecordIntrusiveInterface(record),
			singleton_usable(singleton_usable),
			singleton_bitflag(singleton_bitflag),
			query_data(query_data)
		{
		}

		BitFlagContainerViewer singleton_usable;
		std::span<BitFlag const> singleton_bitflag;
		std::span<std::size_t> query_data;
		std::size_t current_version = 0;

		friend struct Ptr::CurrentWrapper;
	};
}