module;

#include <cassert>

export module NoodlesComponent;

import std;
import Potato;

export import NoodlesMisc;
export import NoodlesArchetype;

export namespace Noodles
{

	export struct ComponentManager;

	struct ChunkView
	{
		Archetype::OPtr archetype;
		std::byte* buffer;
		std::size_t current_count = 0;
		std::size_t max_count = 0;
		std::size_t column_size = 0;
		operator bool() const { return archetype; }
		std::byte* GetComponent(
			Archetype::MemberView const& view,
			std::size_t column_index
		) const;
		std::byte* GetComponent(
			MarkIndex index,
			std::size_t column_index = 0
		) const;
		std::byte* GetComponent(
			Archetype::Index index,
			std::size_t column_index = 0
		) const;
		bool MoveConstructComponent(
			MarkIndex index,
			std::size_t column_index,
			std::byte* target_buffer
		) const;
	};

	struct ChunkInfo
	{

		~ChunkInfo();
		ChunkView GetView() const
		{
			return ChunkView{
			archetype,
				record.GetByte(),
				current_count,
				max_count,
				column_size
			};
		}

		ChunkInfo() = default;
		ChunkInfo(ChunkInfo&&);

	protected:

		Archetype::Ptr archetype;
		Potato::IR::MemoryResourceRecord record;
		std::size_t current_count = 0;
		std::size_t max_count = 0;
		std::size_t column_size = 0;

		friend struct ComponentManager;
	};

	struct ComponentAccessor
	{
		ComponentAccessor(std::span<void*> output_buffer) : output_buffer(output_buffer) {}
		ComponentAccessor(ComponentAccessor const& other, std::span<void*> output_buffer) : archetype(other.archetype), array_size(other.array_size), output_buffer(output_buffer)
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
	protected:
		Archetype::OPtr archetype;
		std::span<void*> output_buffer;
		std::size_t array_size = 0;
		operator bool() const { return archetype; }

		friend struct ComponentManager;
	
	};

	struct ComponentFilter : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<ComponentFilter>;

		StructLayoutMarksInfosView GetRequiredStructLayoutMarks() const
		{
			return {require_write_component, require_component};
		}

		std::span<MarkElement const> GetRefuseStructLayoutMarks() const { return refuse_component; }
		std::span<MarkElement const> GetArchetypeMarkArray() const { return archetype_usable; }
		std::span<MarkIndex const> GetMarkIndex() const { return mark_index; }

		static ComponentFilter::Ptr Create(
			StructLayoutMarkIndexManager& manager,
			std::size_t archetype_storage_count,
			std::span<StructLayoutWriteProperty const> require_component_type,
			std::span<StructLayout::Ptr const> refuse_component_type,
			std::pmr::memory_resource* storage_resource = std::pmr::get_default_resource(),
			std::pmr::memory_resource* archetype_info_resource = std::pmr::get_default_resource()
		);

		bool OnCreatedArchetype(std::size_t archetype_index, Archetype const& archetype);
		std::optional<std::span<std::size_t const>> EnumMountPointIndexByArchetypeIndex_AssumedLocked(std::size_t archetype_index) const;
		std::optional<std::span<std::size_t const>> EnumMountPointIndexByIterator_AssumedLocked(std::size_t iterator, std::size_t& archetype_index) const;

		bool IsIsOverlappingRunTime(ComponentFilter const& other, std::span<MarkElement const> archetype_usage) const;

	protected:

		ComponentFilter(
			Potato::IR::MemoryResourceRecord record, 
			std::span<MarkElement> require_component,
			std::span<MarkElement> require_write_component,
			std::span<MarkElement> refuse_component,
			std::span<MarkElement> archetype_usable,
			std::span<MarkIndex> mark_index,
			std::pmr::memory_resource* resource
		)
			:MemoryResourceRecordIntrusiveInterface(record), require_component(require_component),
			require_write_component(require_write_component),
			refuse_component(refuse_component),
			mark_index(mark_index),
			archetype_member(resource),
			archetype_usable(archetype_usable)
		{
			
		}

		std::span<MarkElement> require_component;
		std::span<MarkElement> require_write_component;
		std::span<MarkElement> refuse_component;
		std::span<MarkElement> archetype_usable;
		std::span<MarkIndex> mark_index;

		mutable std::shared_mutex mutex;
		std::pmr::vector<std::size_t> archetype_member;

		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
		friend struct ComponentManager;
	};

	export struct ComponentManager
	{
		struct Config
		{
			std::size_t component_max_count = 128;
			std::size_t archetype_max_count = 128;
			std::pmr::memory_resource* component_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* archetype_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* resource = std::pmr::get_default_resource();
		};

		ComponentFilter::Ptr CreateComponentFilter(
			std::span<StructLayoutWriteProperty const> require_component,
			std::span<StructLayout::Ptr const> refuse_component,
			std::size_t identity,
			std::pmr::memory_resource* filter_resource = std::pmr::get_default_resource(),
			std::pmr::memory_resource* offset_resource = std::pmr::get_default_resource()
		);

		struct RemovedColumn
		{
			std::size_t archetype_index;
			std::size_t column_index;
		};

		bool ReadComponentRow_AssumedLocked(ComponentFilter const& filter, std::size_t filter_ite, ComponentAccessor& accessor) const;
		bool ReadComponentRow_AssumedLocked(std::size_t archetype_index, ComponentFilter const& filter, ComponentAccessor& accessor) const;
		bool ReadComponent_AssumedLocked(std::size_t archetype_index, std::size_t column_index, ComponentFilter const& filter, ComponentAccessor& accessor) const;

		std::optional<MarkIndex> LocateStructLayout(StructLayout const& loc) { return manager.LocateOrAdd(loc); }
		std::tuple<Archetype::OPtr, OptionalIndex> FindArchetype(std::span<MarkElement const> require_archetype) const
		{
			std::shared_lock sl(chunk_mutex);
			return FindArchetype_AssumedLocked(require_archetype);
		}

		std::tuple<Archetype::OPtr, OptionalIndex> FindArchetype_AssumedLocked(std::span<MarkElement const> require_archetype) const;

		struct ArchetypeBuilderRef
		{
			ArchetypeBuilderRef(ComponentManager& manager, std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());
			ArchetypeBuilderRef(ArchetypeBuilderRef&&) = default;
			bool Insert(StructLayout::Ptr struct_layout, MarkIndex index);
			void Clear();
			std::span<MarkElement> GetMarks() { return mark; }
		protected:
			std::pmr::vector<Archetype::Init> atomic_type;
			std::pmr::vector<MarkElement> mark;
			friend struct ComponentManager;
		};

		std::tuple<Archetype::OPtr, OptionalIndex> FindOrCreateArchetype_AssumedLocked(ArchetypeBuilderRef const& ref)
		{
			auto re = FindArchetype_AssumedLocked(ref.mark);
			if (std::get<0>(re))
				return re;
			return CreateArchetype_AssumedLocked(ref);
		}

		std::tuple<Archetype::OPtr, OptionalIndex> FindOrCreateArchetype(ArchetypeBuilderRef const& ref)
		{
			auto re = FindArchetype(ref.mark);
			if(std::get<0>(re))
				return re;
			std::lock_guard lg(chunk_mutex);
			return FindOrCreateArchetype_AssumedLocked(ref);
		}

		ComponentManager(Config config = {});
		~ComponentManager();
		
		std::shared_mutex& GetMutex() const { return chunk_mutex; }

		bool ReleaseComponentColumn_AssumedLocked(std::size_t archetype_index, std::size_t column_index, std::pmr::vector<RemovedColumn>& removed);
		OptionalIndex AllocateComponentColumn_AssumedLocked(std::size_t archetype_index, std::pmr::vector<RemovedColumn>& removed_list);
		void FixComponentChunkHole_AssumedLocked(std::pmr::vector<RemovedColumn>& holes, void(*func)(ChunkView const& view, std::size_t, std::size_t));
		ChunkView GetChunk_AssumedLocked(std::size_t archetype_index) const;
		std::span<MarkElement const> GetArchetypeUsageMark_AssumedLocked() const { return std::span(archetype_mask).subspan(0, archetype_storage_count); }
		std::span<MarkElement const> GetArchetypeUpdateMark_AssumedLocked() const { return std::span(archetype_mask).subspan(archetype_storage_count); }
		std::size_t GetComponentMarkElementStorageCount() const { return manager.GetStorageCount(); }
		std::size_t GetArchetypeMarkElementStorageCount() const { return archetype_storage_count; }
		void ClearArchetypeUpdateMark_AssumedLocked() { MarkElement::Reset(GetWriteableArchetypeUpdateMark_AssumedLocked()); }
		bool HasArchetypeUpdate_AssumedLocked() const { return !MarkElement::IsReset(GetArchetypeUpdateMark_AssumedLocked()); }

	protected:

		std::span<MarkElement> GetWriteableArchetypeUsageMark_AssumedLocked() { return std::span(archetype_mask).subspan(0, archetype_storage_count); }
		std::span<MarkElement> GetWriteableArchetypeUpdateMark_AssumedLocked() { return std::span(archetype_mask).subspan(archetype_storage_count); }

		std::tuple<Archetype::OPtr, OptionalIndex> CreateArchetype_AssumedLocked(ArchetypeBuilderRef const& ref);

		StructLayoutMarkIndexManager manager;
		std::size_t const archetype_max_count;
		std::size_t const archetype_storage_count;
		struct FilterElement
		{
			ComponentFilter::Ptr filter;
			OptionalIndex identity;
		};

		mutable std::shared_mutex chunk_mutex;
		std::pmr::vector<ChunkInfo> chunk_infos;
		std::pmr::unsynchronized_pool_resource archetype_resource;
		std::pmr::unsynchronized_pool_resource component_resource;
		std::pmr::vector<MarkElement> archetype_mask;

		mutable std::shared_mutex filter_mutex;
		std::pmr::vector<FilterElement> filters;
	};
}