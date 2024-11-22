module;

#include <cassert>

export module NoodlesComponent;

import std;
import PotatoTMP;
import PotatoMisc;
import PotatoPointer;
import PotatoIR;
import PotatoTaskSystem;

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
		operator bool() const { return buffer != nullptr; }
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

	struct ComponentRowWrapper
	{
		Archetype::OPtr archetype;
		std::pmr::vector<void*> component_row_buffers;
		std::size_t array_size;
		operator bool() const { return archetype; }
		template<typename Type>
		std::span<Type> AsSpan(std::size_t index){ return std::span{
			static_cast<Type*>(component_row_buffers[index]),
			array_size
		};}
	};

	struct ComponentFilter : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<ComponentFilter>;


		std::span<MarkElement const> GetRequiredAtomicMarkArray() const { return require_component; }
		std::span<MarkElement const> GetRequiresWriteAtomicMarkArray() const { return require_write_component; }
		std::span<MarkElement const> GetRefuseAtomicMarkArray() const { return refuse_component; }
		std::span<MarkIndex const> GetMarkIndex() const { return mark_index; }

		struct Info
		{
			bool need_write = false;
			StructLayout::Ptr struct_layout;
		};

		static ComponentFilter::Ptr Create(
			StructLayoutMarkIndexManager& manager,
			std::span<Info const> require_component_type,
			std::span<StructLayout::Ptr const> refuse_component_type,
			std::pmr::memory_resource* storage_resource = std::pmr::get_default_resource(),
			std::pmr::memory_resource* archetype_info_resource = std::pmr::get_default_resource()
		);

		bool OnCreatedArchetype(std::size_t archetype_index, Archetype const& archetype);
		std::optional<std::span<std::size_t const>> EnumMountPointIndexByArchetypeIndex_AssumedLocked(std::size_t archetype_index) const;
		std::optional<std::span<std::size_t const>> EnumMountPointIndexByIterator_AssumedLocked(std::size_t iterator, std::size_t& archetype_index) const;

	protected:

		ComponentFilter(
			Potato::IR::MemoryResourceRecord record, 
			std::span<MarkElement> require_component,
			std::span<MarkElement> require_write_component,
			std::span<MarkElement> refuse_component,
			std::span<MarkIndex> mark_index,
			std::pmr::memory_resource* resource
		)
			:MemoryResourceRecordIntrusiveInterface(record), require_component(require_component),
			require_write_component(require_write_component),
			refuse_component(refuse_component),
			mark_index(mark_index),
			archetype_member(resource)
		{
			
		}

		std::span<MarkElement> require_component;
		std::span<MarkElement> require_write_component;
		std::span<MarkElement> refuse_component;
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
			std::size_t component_atomic_type_count = 128;
			std::size_t archetype_count = 128;
			std::pmr::memory_resource* component_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* archetype_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* resource = std::pmr::get_default_resource();
		};

		ComponentFilter::Ptr CreateComponentFilter(
			std::span<ComponentFilter::Info const> require_component,
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

		std::optional<ComponentRowWrapper> ReadComponentRow_AssumedLocked(ComponentFilter const& filter, std::size_t filter_ite, std::pmr::memory_resource* wrapper_resource = std::pmr::get_default_resource()) const;
		std::optional<ComponentRowWrapper> ReadComponentRow_AssumedLocked(std::size_t archetype_index, ComponentFilter const& filter, std::pmr::memory_resource* wrapper_resource = std::pmr::get_default_resource()) const;
		std::optional<ComponentRowWrapper> ReadComponent_AssumedLocked(std::size_t archetype_index, std::size_t column_index, ComponentFilter const& filter, std::pmr::memory_resource* wrapper_resource = std::pmr::get_default_resource()) const;

		std::optional<MarkIndex> LocateStructLayout(StructLayout::Ptr const& loc) { return manager.LocateOrAdd(loc); }
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
		StructLayoutMarkIndexManager& GetAtomicTypeManager() { return manager; }
		StructLayoutMarkIndexManager const& GetAtomicTypeManager() const { return manager; }

		std::shared_mutex& GetMutex() const { return chunk_mutex; }

		bool ReleaseComponentColumn_AssumedLocked(std::size_t archetype_index, std::size_t column_index, std::pmr::vector<RemovedColumn>& removed);
		OptionalIndex AllocateComponentColumn_AssumedLocked(std::size_t archetype_index, std::pmr::vector<RemovedColumn>& removed_list);
		void FixComponentChunkHole_AssumedLocked(std::pmr::vector<RemovedColumn>& holes, void(*func)(ChunkView const& view, std::size_t, std::size_t));
		ChunkView GetChunk_AssumedLocked(std::size_t archetype_index) const;

	protected:

		std::tuple<Archetype::OPtr, OptionalIndex> CreateArchetype_AssumedLocked(ArchetypeBuilderRef const& ref);

		StructLayoutMarkIndexManager manager;

		struct FilterElement
		{
			ComponentFilter::Ptr filter;
			OptionalIndex identity;
		};

		std::size_t archetype_count;

		mutable std::shared_mutex chunk_mutex;
		std::pmr::vector<ChunkInfo> chunk_infos;
		std::pmr::unsynchronized_pool_resource archetype_resource;
		std::pmr::unsynchronized_pool_resource component_resource;
		std::pmr::vector<MarkElement> archetype_mask;

		mutable std::shared_mutex filter_mutex;
		std::pmr::vector<FilterElement> filters;
	};
}