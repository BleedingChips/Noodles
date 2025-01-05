module;

#include <cassert>

export module NoodlesQuery;

import std;
import Potato;

import NoodlesArchetype;

export namespace Noodles
{
	struct QueryData
	{
		QueryData(std::span<void*> output_buffer) : output_buffer(output_buffer) {}
		QueryData(QueryData const& other, std::span<void*> output_buffer) : archetype(other.archetype), array_size(other.array_size), output_buffer(output_buffer)
		{
			assert(output_buffer.size() == other.output_buffer.size());
			std::memcpy(output_buffer.data(), other.output_buffer.data(), sizeof(void*) * output_buffer.size());
		}
		template<typename Type>
		std::span<Type> AsSpan(std::size_t index) const {
			return std::span{
				static_cast<Type*>(output_buffer[index]),
				array_size
			};
		}

		Archetype::OPtr archetype;
		std::span<void*> output_buffer;
		std::size_t array_size = 0;
		operator bool() const { return archetype; }
	};

	struct ComponentQuery : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<ComponentQuery>;

		StructLayoutMarksInfosView GetRequiredStructLayoutMarks() const
		{
			return { require_write_component, require_component };
		}

		std::span<MarkElement const> GetRefuseStructLayoutMarks() const { return refuse_component; }
		std::span<MarkElement const> GetArchetypeMarkArray() const { return archetype_usable; }
		std::span<MarkIndex const> GetComponentMarkIndex() const { return mark_index; }

		static ComponentQuery::Ptr Create(
			StructLayoutManager& manager,
			std::span<StructLayoutWriteProperty const> require_component_type,
			std::span<StructLayout::Ptr const> refuse_component_type,
			std::pmr::memory_resource* resource = std::pmr::get_default_resource()
		);


		std::optional<std::span<std::size_t const>> EnumMountPointIndexByArchetypeIndex_AssumedLocked(std::size_t archetype_index) const;
		std::optional<std::span<std::size_t const>> EnumMountPointIndexByIterator_AssumedLocked(std::size_t iterator, std::size_t& archetype_index) const;

		bool IsIsOverlappingRunTime(ComponentQuery const& other, std::span<MarkElement const> archetype_usage) const;

		std::shared_mutex& GetMutex() const { return mutex; }

		std::optional<std::size_t> Update_AssumedLocked(StructLayoutManager& manager, std::size_t chunk_id, std::size_t chunk_size);
		bool VersionCheck_AssumedLocked(StructLayoutManager& manager, std::size_t chunk_id, std::size_t chunk_size) const;
		bool OnCreatedArchetype_AssumedLocked(std::size_t archetype_index, Archetype const& archetype);

	protected:

		ComponentQuery(
			Potato::IR::MemoryResourceRecord record,
			std::span<MarkElement> require_component,
			std::span<MarkElement> require_write_component,
			std::span<MarkElement> refuse_component,
			std::span<MarkElement> archetype_usable,
			std::span<MarkIndex> mark_index,
			StructLayoutManager::Ptr manager,
			std::pmr::memory_resource* resource
		)
			:MemoryResourceRecordIntrusiveInterface(record), require_component(require_component),
			require_write_component(require_write_component),
			refuse_component(refuse_component),
			mark_index(mark_index),
			archetype_member(resource),
			archetype_usable(archetype_usable),
			manager(std::move(manager))
		{

		}

		std::span<MarkElement> require_component;
		std::span<MarkElement> require_write_component;
		std::span<MarkElement> refuse_component;
		std::span<MarkElement> archetype_usable;
		std::span<MarkIndex> mark_index;

		mutable std::shared_mutex mutex;
		std::pmr::vector<std::size_t> archetype_member;
		StructLayoutManager::Ptr const manager;
		std::size_t chunk_id = 0;
		std::size_t chunk_size_last_update = 0;

		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
	};
}