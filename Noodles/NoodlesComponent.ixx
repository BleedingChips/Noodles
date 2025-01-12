module;

#include <cassert>

export module NoodlesComponent;

import std;
import Potato;

export import NoodlesMisc;
export import NoodlesArchetype;
import NoodlesQuery;

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

	export struct ComponentManager
	{
		struct Config
		{
			std::pmr::memory_resource* component_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* archetype_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* resource = std::pmr::get_default_resource();
		};

		struct RemovedColumn
		{
			std::size_t archetype_index;
			std::size_t column_index;
		};

		bool ReadComponentRow_AssumedLocked(ComponentQuery const& query, std::size_t filter_ite, QueryData& data) const;
		bool ReadComponentRow_AssumedLocked(std::size_t archetype_index, ComponentQuery const& query, QueryData& accessor) const;
		bool ReadComponent_AssumedLocked(std::size_t archetype_index, std::size_t column_index, ComponentQuery const& query, QueryData& accessor) const;

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

		ComponentManager(StructLayoutManager&layout_manager, Config config = {});
		~ComponentManager();
		
		std::shared_mutex& GetMutex() const { return chunk_mutex; }

		bool ReleaseComponentColumn_AssumedLocked(std::size_t archetype_index, std::size_t column_index, std::pmr::vector<RemovedColumn>& removed);
		OptionalIndex AllocateComponentColumn_AssumedLocked(std::size_t archetype_index, std::pmr::vector<RemovedColumn>& removed_list);
		void FixComponentChunkHole_AssumedLocked(std::pmr::vector<RemovedColumn>& holes, void(*func)(void* data, ChunkView const& view, std::size_t, std::size_t), void* data);
		ChunkView GetChunk_AssumedLocked(std::size_t archetype_index) const;
		std::span<MarkElement const> GetArchetypeUsageMark_AssumedLocked() const { return std::span(archetype_mask).subspan(0, manager->GetArchetypeStorageCount()); }
		std::span<MarkElement const> GetArchetypeUpdateMark_AssumedLocked() const { return std::span(archetype_mask).subspan(manager->GetArchetypeStorageCount()); }
		void ClearArchetypeUpdateMark_AssumedLocked() { MarkElement::Reset(GetWriteableArchetypeUpdateMark_AssumedLocked()); }
		bool HasArchetypeUpdate_AssumedLocked() const { return !MarkElement::IsReset(GetArchetypeUpdateMark_AssumedLocked()); }
		bool CheckVersion_AssumedLocked(ComponentQuery const& query) const;
		bool UpdateFilter_AssumedLocked(ComponentQuery& query) const;

	protected:

		std::span<MarkElement> GetWriteableArchetypeUsageMark_AssumedLocked() { return std::span(archetype_mask).subspan(0, manager->GetArchetypeStorageCount()); }
		std::span<MarkElement> GetWriteableArchetypeUpdateMark_AssumedLocked() { return std::span(archetype_mask).subspan(manager->GetArchetypeStorageCount()); }

		std::tuple<Archetype::OPtr, OptionalIndex> CreateArchetype_AssumedLocked(ArchetypeBuilderRef const& ref);

		StructLayoutManager::Ptr manager;

		mutable std::shared_mutex chunk_mutex;
		std::pmr::vector<ChunkInfo> chunk_infos;
		std::pmr::unsynchronized_pool_resource archetype_resource;
		std::pmr::unsynchronized_pool_resource component_resource;
		std::pmr::vector<MarkElement> archetype_mask;
	};
}