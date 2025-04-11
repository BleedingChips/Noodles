module;

#include <cassert>

export module NoodlesComponent;

import std;
import Potato;

import NoodlesMisc;
import NoodlesArchetype;
import NoodlesBitFlag;
import NoodlesGlobalContext;


export namespace Noodles
{
	struct Chunk
	{
		using Ptr = Potato::Pointer::UniquePtr<Chunk>;

		static Ptr Create(Archetype const& target_archetype, std::size_t suggest_component_count, std::pmr::memory_resource* resource);
		std::byte* GetComponent(Archetype::MemberView const& member, std::size_t component_index);
		void DestructComponent(Archetype const& target_archetype, std::size_t component_index);
		void MoveConstructComponent(Archetype const& target_archetype, std::size_t self_component_index, Chunk& source_chunk, std::size_t source_component_index);
		void DestructAllComponent(Archetype const& target_archetype);
		OptionalSizeT AllocateComponentWithoutConstruct();
		OptionalSizeT PopComponentWithoutDestruct();

		~Chunk() = default;

	protected:

		void Release();

		Chunk(Potato::IR::MemoryResourceRecord record, std::size_t max_count, std::byte* buffer)
			: record(record), max_count(max_count), buffer(buffer)
		{

		}

		Potato::IR::MemoryResourceRecord record;
		std::size_t current_count = 0;
		std::size_t max_count = 0;
		std::size_t column_size = 0;
		std::byte* buffer = nullptr;

		friend struct Ptr::CurrentWrapper;
	};

	/*
	struct ArchetypeBuilderRef
	{
		ArchetypeBuilderRef(StructBitFlagMapping& mapping, std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());
		ArchetypeBuilderRef(ArchetypeBuilderRef&&) = default;
		bool Insert(StructLayout::Ptr struct_layout, BitFlag bitflag, std::byte* buffer);
		bool Remove(BitFlag bitflag);
		void Clear();
		std::span<BitFlagContainer> GetMarks() { return mark; }
		std::span<BitFlagContainer const> GetMarks() const { return mark; }
	protected:
		std::pmr::vector<Archetype::Init> atomic_type;
		std::pmr::vector<std::byte*> reference_buffer;
		std::pmr::vector<BitFlagContainer> mark;
		StructBitFlagMapping::Ptr bit_flag_mapping;
		friend struct ComponentManager;
	};
	*/

	struct ComponentChunkManager
	{
		struct Config
		{
			std::size_t max_archetype_count = 128;
			std::pmr::memory_resource* component_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* resource = std::pmr::get_default_resource();
		};

		BitFlagConstContainer GetComponentChunkUpdateBitFlag() const { return component_chunk_update_bitflag; }
		BitFlagConstContainer GetComponentChunkNotEmptyBitFlag() const { return component_chunk_not_empty_bitflag; }

		OptionalSizeT LocateComponentChunk(BitFlagConstContainer component_bitflag) const;
		OptionalSizeT CreateComponentChunk(std::span<Archetype::Init const> archetype_init);

		/*
		OptionalSizeT LocateComponentChunk(BitFlagConstContainer component_bitflag) const;
		ComponentChunkOPtr GetComponentChunk(std::size_t component_chunk_index) const;
		OptionalSizeT CreateComponentChunk(std::span<Archetype::Init const> archetype_init);
		*/

		ComponentChunkManager(GlobalContext::Ptr global_context, Config config = {});
		~ComponentChunkManager();
		
		//std::shared_mutex& GetMutex() const { return chunk_mutex; }

		//bool ReleaseComponentColumn_AssumedLocked(std::size_t archetype_index, std::size_t column_index, std::pmr::vector<RemovedColumn>& removed);
		//OptionalIndex AllocateComponentColumn_AssumedLocked(std::size_t archetype_index, std::pmr::vector<RemovedColumn>& removed_list);
		//void FixComponentChunkHole_AssumedLocked(std::pmr::vector<RemovedColumn>& holes, void(*func)(void* data, ChunkView const& view, std::size_t, std::size_t), void* data);
		//ChunkView GetChunk_AssumedLocked(std::size_t archetype_index) const;
		/*
		
		void ClearArchetypeUpdateMark() { MarkElement::Reset(archetype_update_mask); }
		bool HasArchetypeUpdate() const { return !MarkElement::IsReset(GetArchetypeUpdateMark()); }
		//bool CheckVersion_AssumedLocked(ComponentQuery const& query) const;
		//bool UpdateFilter_AssumedLocked(ComponentQuery& query) const;
		*/
	protected:

		//std::tuple<Archetype::OPtr, OptionalIndex> CreateArchetype_AssumedLocked(ArchetypeBuilderRef const& ref);

		GlobalContext::Ptr global_context;

		struct ChunkInfo
		{
			Archetype::Ptr archtype;
			Potato::Misc::IndexSpan<> chunk_span;
			std::size_t total_components = 0;
		};

		std::pmr::vector<ChunkInfo> component_chunks_info;
		std::pmr::vector<Chunk::Ptr> chunks;

		std::pmr::unsynchronized_pool_resource archetype_resource;
		std::pmr::unsynchronized_pool_resource component_resource;

		std::pmr::vector<BitFlagContainer::Element> bit_flag_container;
		BitFlagContainer component_chunk_update_bitflag;
		BitFlagContainer component_chunk_not_empty_bitflag;
		std::size_t max_archtype_count = 0;
	};
}