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
		bool DestructComponent(Archetype const& target_archetype, std::size_t component_index);
		void MoveConstructComponent(Archetype const& target_archetype, std::size_t self_component_index, Chunk& source_chunk, std::size_t source_component_index);
		void DestructAllComponent(Archetype const& target_archetype);
		bool DestructSingleComponent(Archetype const& target_archetype, std::size_t component_index, BitFlag target_component);
		bool ConstructComponent(Archetype const& target_archetype, std::size_t component_index, BitFlag target_component, std::byte* target_buffer, bool is_move_construction, bool need_destruct_before_construct);
		OptionalSizeT AllocateComponentWithoutConstruct();
		OptionalSizeT PopComponentWithoutDestruct();
		std::size_t GetComponentCount() const { return current_count; }
		~Chunk() = default;
		OptionalSizeT PopBack(Archetype const& target_archetype);

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

	struct ComponentManager
	{

		struct Index
		{
			OptionalSizeT archetype_index;
			std::size_t chunk_index;
			std::size_t component_index;
			operator bool() const { return archetype_index; }
			std::strong_ordering operator<=>(Index const&) const noexcept = default;
		};

		struct Config
		{
			std::size_t max_archetype_count = 128;
			std::pmr::memory_resource* component_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* resource = std::pmr::get_default_resource();
		};

		BitFlagConstContainer GetArchetypeUpdateBitFlag() const { return archetype_update_bitflag; }
		BitFlagConstContainer GetArchetypeNotEmptyBitFlag() const { return archetype_not_empty_bitflag; }

		OptionalSizeT LocateComponentChunk(BitFlagConstContainer component_bitflag) const;
		OptionalSizeT CreateComponentChunk(std::span<Archetype::Init const> archetype_init);

		/*
		OptionalSizeT LocateComponentChunk(BitFlagConstContainer component_bitflag) const;
		ComponentChunkOPtr GetComponentChunk(std::size_t component_chunk_index) const;
		OptionalSizeT CreateComponentChunk(std::span<Archetype::Init const> archetype_init);
		*/

		ComponentManager(GlobalContext::Ptr global_context, Config config = {});
		~ComponentManager();
		
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

		bool DestructionComponent(Index index);

		struct Init
		{
			BitFlag component_class = BitFlag{std::numeric_limits<std::size_t>::max()};
			bool move_construct = false;
			std::byte* data = nullptr;
		};

		struct ConstructOption
		{
			bool destruct_before_construct = false;
			bool full_construction = false;
		};

		bool ConstructComponent(Index index, std::span<Init const> component_init, ConstructOption option);

		std::size_t FlushComponentInitWithComponent(Index index, BitFlagConstContainer component_class, std::span<Init> in_out, std::span<Archetype::Init> in_out_archetype);
		Index AllocateNewComponentWithoutConstruct(std::size_t archetype_index);

		std::optional<bool> PopBackComponentToFillHole(Index hole_index, Index pop_back_limited);
		void ClearBitFlag();

	protected:

		//std::tuple<Archetype::OPtr, OptionalIndex> CreateArchetype_AssumedLocked(ArchetypeBuilderRef const& ref);

		GlobalContext::Ptr global_context;

		struct ArchetypeInfo
		{
			Archetype::Ptr archtype;
			Potato::Misc::IndexSpan<> chunk_span;
			std::size_t total_components = 0;
		};

		std::pmr::vector<ArchetypeInfo> archetype_info;
		std::pmr::vector<Chunk::Ptr> chunks;

		std::pmr::unsynchronized_pool_resource archetype_resource;
		std::pmr::unsynchronized_pool_resource component_resource;

		std::pmr::vector<BitFlagContainer::Element> bit_flag_container;
		BitFlagContainer archetype_update_bitflag;
		BitFlagContainer archetype_not_empty_bitflag;
		std::size_t max_archtype_count = 0;
	};
}