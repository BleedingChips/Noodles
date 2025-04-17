module;

#include <cassert>

export module NoodlesQuery;

import std;
import Potato;

import NoodlesArchetype;
import NoodlesBitFlag;
import NoodlesComponent;
import NoodlesGlobalContext;

export namespace Noodles
{

	struct ComponentQueryManager
	{

		void RegisterQueryIndex(std::size_t query_index);
		void UnRegisterQuery(std::size_t query_index);
		std::span<std::size_t const> GetQueryData(std::size_t query_index);
		void GetUpdateFromComponent(ComponentManager& manager);

	protected:

		struct QueryInfo
		{
			std::size_t index = 0;
			std::size_t reference_count = 0;

			Potato::Misc::IndexSpan<> class_offset;
			Potato::Misc::IndexSpan<> mapping_data;
		};

		std::pmr::vector<QueryInfo> query_info;
		std::pmr::vector<BitFlag> container;
		std::pmr::vector<std::size_t> offset_or_size;
		std::pmr::vector<BitFlagContainer::Element> archetype_usage;
	};








	/*
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
		bool VersionCheck_AssumedLocked(std::size_t chunk_id, std::size_t chunk_size) const;
		bool VersionCheck_AssumedLocked(std::size_t chunk_id) const;
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

	struct SingletonQuery : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{

		using Ptr = Potato::Pointer::IntrusivePtr<SingletonQuery>;

		StructLayoutMarksInfosView GetRequiredStructLayoutMarks() const
		{
			return { require_write_singleton, require_singleton };
		}

		std::span<MarkIndex const> GetMarkIndex() const { return mark_index; }


		void Reset();

		static SingletonQuery::Ptr Create(
			StructLayoutManager& manager,
			std::span<StructLayoutWriteProperty const> require_singleton,
			std::pmr::memory_resource* storage_resource = std::pmr::get_default_resource()
		);

		std::span<std::size_t const> EnumSingleton_AssumedLocked() const { return archetype_offset; }
		bool OnSingletonModify_AssumedLocked(Archetype const& archetype);
		bool Update_AssumedLocked(StructLayoutManager& manager, std::size_t archetype_id);
		bool VersionCheck_AssumedLocked(std::size_t archetype_id) const;
		std::shared_mutex& GetMutex() const { return mutex; }

	protected:

		SingletonQuery(
			Potato::IR::MemoryResourceRecord record,
			std::span<MarkElement> require_singleton,
			std::span<MarkElement> require_write_singleton,
			std::span<MarkIndex> mark_index,
			std::span<std::size_t> archetype_offset,
			StructLayoutManager::Ptr manager
		)
			:MemoryResourceRecordIntrusiveInterface(record), require_singleton(require_singleton),
			require_write_singleton(require_write_singleton),
			mark_index(mark_index),
			archetype_offset(archetype_offset),
			manager(std::move(manager))
		{
		}


		StructLayoutManager::Ptr const manager;
		std::span<MarkElement> require_singleton;
		std::span<MarkElement> require_write_singleton;
		std::span<MarkIndex> mark_index;

		mutable std::shared_mutex mutex;
		std::span<std::size_t> archetype_offset;
		std::size_t archetype_id = 0;

		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
	};

	struct ThreadOrderQuery : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		StructLayoutMarksInfosView GetStructLayoutMarks() const { return marks; };
		using Ptr = Potato::Pointer::IntrusivePtr<ThreadOrderQuery>;

		static Ptr Create(StructLayoutManager& manager, std::span<StructLayoutWriteProperty const> info, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

	protected:
		ThreadOrderQuery(Potato::IR::MemoryResourceRecord record, StructLayoutMarksInfos marks)
			: MemoryResourceRecordIntrusiveInterface(record), marks(marks) {
		}
		StructLayoutMarksInfos marks;




		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
	};
	*/
}