module;

#include <cassert>

export module NoodlesComponent;

import std;
import Potato;

import NoodlesMisc;
import NoodlesArchetype;
import NoodlesBitFlag;
import NoodlesClassBitFlag;


export namespace Noodles
{
	struct Chunk
	{
		using Ptr = Potato::Pointer::UniquePtr<Chunk>;

		static Ptr Create(Archetype const& target_archetype, std::size_t suggest_component_count, std::pmr::memory_resource* resource);
		void* GetComponent(Archetype::MemberView const& member, std::size_t entity_index);
		void* GetComponent(std::size_t member_offset, std::size_t member_size, std::size_t entity_index);
		void* GetComponentArray(std::size_t member_offset);
		bool DestructEntity(Archetype const& target_archetype, std::size_t entity_index);
		void MoveConstructEntity(Archetype const& target_archetype, std::size_t entity_index, Chunk& source_chunk, std::size_t source_entity_index);
		void DestructAll(Archetype const& target_archetype);
		bool DestructComponent(Archetype const& target_archetype, std::size_t entity_index, BitFlag target_component);
		bool ConstructComponent(Archetype const& target_archetype, std::size_t entity_index, BitFlag target_component, void* target_buffer, bool is_move_construction, bool need_destruct_before_construct);
		OptionalSizeT AllocateEntityWithoutConstruct();
		OptionalSizeT PopEntityWithoutDestruct();
		std::size_t GetEntityCount() const { return current_count; }
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

	struct ComponentManager
	{

		struct Index
		{
			std::size_t archetype_index = 0;
			std::size_t chunk_index = 0;
			std::size_t entity_index = 0;
			Index(std::size_t archetype_index, std::size_t chunk_index, std::size_t entity_index)
				: archetype_index(archetype_index), chunk_index(chunk_index), entity_index(entity_index) {}
			Index(Index const&) = default;
			std::strong_ordering operator<=>(Index const&) const noexcept = default;
		};

		struct Config
		{
			std::size_t max_archetype_count = 128;
			std::size_t max_component_class_count = 128;
			std::pmr::memory_resource* component_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* resource = std::pmr::get_default_resource();
		};

		BitFlagContainerConstViewer GetArchetypeUpdateBitFlag() const { return archetype_update_bitflag; }
		BitFlagContainerConstViewer GetArchetypeNotEmptyBitFlag() const { return archetype_not_empty_bitflag; }
		bool HasArchetypeCreated() const { return has_new_archetype; }

		OptionalSizeT LocateComponentChunk(BitFlagContainerConstViewer component_bitflag) const;
		OptionalSizeT CreateComponentChunk(std::span<Archetype::Init const> archetype_init);

		std::size_t GetArchetypeCount() const { return archetype_info.size(); }
		std::size_t GetArchetypeBitFlagContainerCount() const { return archetype_update_bitflag.AsSpan().size(); }

		static constexpr std::size_t GetQueryDataCount() { return 2; }

		ComponentManager(Config config = {});
		~ComponentManager();

		bool DestructionEntity(Index index);

		struct Init
		{
			BitFlag component_class = BitFlag{std::numeric_limits<std::size_t>::max()};
			bool move_construct = false;
			void* data = nullptr;
		};

		struct ConstructOption
		{
			bool destruct_before_construct = false;
			bool full_construction = false;
		};

		bool ConstructEntity(Index index, std::span<Init const> component_init, ConstructOption option);

		std::size_t FlushInitWithComponent(Index index, BitFlagContainerConstViewer component_class, std::span<Init> in_out, std::span<Archetype::Init> in_out_archetype);
		std::optional<Index> AllocateEntityWithoutConstruct(std::size_t archetype_index);

		std::optional<bool> PopBackEntityToFillHole(Index hole_index, Index pop_back_limited);

		void ResetUpdatedState();
		void UpdateUpdatedState();
		
		bool IsArchetypeAcceptQuery(std::size_t archetype_index, BitFlagContainerConstViewer query_class, BitFlagContainerConstViewer refuse_qurey_class) const;
		bool TranslateClassToQueryData(std::size_t archetype_index, std::span<BitFlag const> target_class, std::span<std::size_t> outoput_query_data) const;
		bool QueryComponent(Index index, std::span<std::size_t const> query_data, std::span<void*> output) const;
		OptionalSizeT QueryComponentArray(std::size_t archetype_index, std::size_t chunk_index, std::span<std::size_t const> query_data, std::span<void*> output) const;
		static void Sort(std::span<Archetype::Init> init_list);
		OptionalSizeT GetChunkCount(std::size_t archetype_index) const;

	protected:

		//std::tuple<Archetype::OPtr, OptionalIndex> CreateArchetype_AssumedLocked(ArchetypeBuilderRef const& ref);

		std::size_t const component_bitflag_container_element_count;

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
		BitFlagContainerViewer archetype_update_bitflag;
		BitFlagContainerViewer archetype_not_empty_bitflag;
		BitFlagContainerViewer archetype_not_empty_bitflag_lastframe;
		bool has_new_archetype = false;
		std::size_t max_archetype_count = 0;
	};
}