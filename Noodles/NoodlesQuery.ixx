module;

#include <cassert>

export module NoodlesQuery;

import std;
import Potato;

import NoodlesArchetype;
import NoodlesBitFlag;
import NoodlesComponent;
import NoodlesSingleton;
import NoodlesEntity;

export namespace Noodles
{

	template<typename InitFunction>
	concept ComponentQueryInitFunction = std::is_invocable_v<std::remove_cvref_t<InitFunction>, std::span<BitFlag>, BitFlagContainerViewer, BitFlagContainerViewer>;

	struct ComponentQuery : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<ComponentQuery>;
		using OPtr = Potato::Pointer::ObserverPtr<ComponentQuery>;

		std::span<BitFlag const> GetRequireBitFlag() const { return require_bitflag; }
		BitFlagContainerConstViewer GetRequireContainerConstViewer() const { return require_bitflag_viewer; }
		BitFlagContainerConstViewer GetWritedContainerConstViewer() const { return writed_bitflag_viewer; }
		BitFlagContainerConstViewer GetArchetypeContainerConstViewer() const { return archetype_bitflag_viewer; }

		template<ComponentQueryInitFunction InitFunction>
		static Ptr Create(
			std::size_t archetype_container_count,
			std::size_t component_container_count,
			std::size_t require_count,
			InitFunction&& function,
			std::pmr::memory_resource* resource = std::pmr::get_default_resource()
		)
		{
			return Create(
				archetype_container_count,
				component_container_count,
				require_count,
				[](void* data, std::span<BitFlag> require, BitFlagContainerViewer writed, BitFlagContainerViewer refuse) {
					(*static_cast<InitFunction*>(data))(require, writed, refuse);
				},
				&function,
				resource
			);
		}

		static Ptr Create(
			std::size_t archetype_container_count, 
			std::size_t component_container_count,
			std::size_t require_count,
			void (*init_func)(void*, std::span<BitFlag> require, BitFlagContainerViewer writed, BitFlagContainerViewer refuse),
			void* append_data,
			std::pmr::memory_resource* resource = std::pmr::get_default_resource()
		);

		bool UpdateQueryData(ComponentManager const& manager);

		std::size_t GetArchetypeCount() const { return archetype_count; }

		std::optional<std::size_t> QueryComponentArrayWithIterator(ComponentManager& manager, std::size_t iterator, std::size_t chunk_index, std::span<void*> output_component) const;
		std::optional<std::size_t> GetChunkCount(ComponentManager& manager, std::size_t iterator) const;
		bool QueryComponent(ComponentManager& manager, ComponentManager::Index component_index, std::span<void*> output_component) const;
		~ComponentQuery() = default;

	protected:

		ComponentQuery(
			Potato::IR::MemoryResourceRecord record,
			BitFlagContainerConstViewer require_bitflag_viewer,
			BitFlagContainerConstViewer writed_bitflag_viewer,
			BitFlagContainerConstViewer refuse_bitflag_viewer,
			BitFlagContainerViewer archetype_bitflag_viewer,
			std::span<BitFlag const> require_bitflag
		)
			:MemoryResourceRecordIntrusiveInterface(record), 
			require_bitflag_viewer(require_bitflag_viewer),
			writed_bitflag_viewer(writed_bitflag_viewer),
			refuse_bitflag_viewer(refuse_bitflag_viewer),
			archetype_bitflag_viewer(archetype_bitflag_viewer),
			require_bitflag(require_bitflag),
			query_data(record.GetMemoryResource())
		{
			query_data_fast_offset = require_bitflag.size() * ComponentManager::GetQueryDataCount() + 1;
		}

		BitFlagContainerConstViewer require_bitflag_viewer;
		BitFlagContainerConstViewer writed_bitflag_viewer;
		BitFlagContainerConstViewer refuse_bitflag_viewer;
		BitFlagContainerViewer archetype_bitflag_viewer;
		std::span<BitFlag const> require_bitflag;
		std::pmr::vector<std::size_t> query_data;
		std::size_t archetype_count = 0;
		std::size_t updated_archetype_count = 0;
		std::size_t query_data_fast_offset = 0;

		friend struct Ptr::CurrentWrapper;
	};

	template<typename InitFunction>
	concept SingletonQueryInitFunction = std::is_invocable_v<std::remove_cvref_t<InitFunction>, std::span<BitFlag>, BitFlagContainerViewer>;

	struct SingletonQuery : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<SingletonQuery>;
		using OPtr = Potato::Pointer::ObserverPtr<SingletonQuery>;

		std::span<BitFlag const> GetRequireBitFlag() const { return singleton_bitflag; }
		BitFlagContainerConstViewer GetRequireContainerConstViewer() const { return require_bitflag_viewer; }
		BitFlagContainerConstViewer GetWritedContainerConstViewer() const { return write_bitflag_viewer; }
		
		template<SingletonQueryInitFunction InitFunction>
		static Ptr Create(
			std::size_t singleton_container_count,
			std::size_t singleton_count,
			InitFunction&& function,
			std::pmr::memory_resource* resource = std::pmr::get_default_resource()
		)
		{
			return Create(
				singleton_container_count,
				singleton_count,
				[](void* data, std::span<BitFlag> require, BitFlagContainerViewer writed) {
					(*static_cast<InitFunction*>(data))(require, writed);
				},
				&function,
				resource
			);
		}

		static Ptr Create(
			std::size_t singleton_container_count,
			std::size_t singleton_count,
			void (*init_func)(void*, std::span<BitFlag> require, BitFlagContainerViewer writed),
			void* append_data,
			std::pmr::memory_resource* resource = std::pmr::get_default_resource()
		);

		bool UpdateQueryData(SingletonManager const& manager);

		bool QuerySingleton(SingletonManager const& manager, std::span<void*> output_component) const;

		~SingletonQuery() = default;

	protected:

		SingletonQuery(
			Potato::IR::MemoryResourceRecord record,
			BitFlagContainerViewer require_bitflag_viewer,
			BitFlagContainerViewer write_bitflag_viewer,
			std::span<BitFlag const> singleton_bitflag,
			std::span<std::size_t> query_data
		)
			:MemoryResourceRecordIntrusiveInterface(record),
			require_bitflag_viewer(require_bitflag_viewer),
			write_bitflag_viewer(write_bitflag_viewer),
			singleton_bitflag(singleton_bitflag),
			query_data(query_data)
		{
		}

		BitFlagContainerViewer require_bitflag_viewer;
		BitFlagContainerViewer write_bitflag_viewer;
		std::span<BitFlag const> singleton_bitflag;
		std::span<std::size_t> query_data;
		std::size_t current_version = 0;

		friend struct Ptr::CurrentWrapper;
	};
}