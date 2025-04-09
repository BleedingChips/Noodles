module;

export module NoodlesChunk;

import std;
import Potato;

export import NoodlesMisc;
export import NoodlesArchetype;

export namespace Noodles
{
	struct Chunk
	{
		using Ptr = Potato::Pointer::UniquePtr<Chunk>;
		using OPtr = Potato::Pointer::ObserverPtr<Chunk>;

		static Ptr Create(Archetype const& target_archetype, std::size_t suggest_component_count, std::pmr::memory_resource* resource);
		std::byte* GetComponent(Archetype::MemberView const& member, std::size_t component_index);
		void DestructComponent(Archetype const& target_archetype, std::size_t component_index);
		void MoveConstructComponent(Archetype const& target_archetype,  std::size_t self_component_index, Chunk& source_chunk, std::size_t source_component_index);
		void DestructAllComponent(Archetype const& target_archetype);
		OptionalSizeT AllocateComponentWithoutConstruct();
		OptionalSizeT PopComponentWithoutDestruct();
		Ptr MoveNextChunk() { return std::move(next_chunk); }
		void ReleaseLastChunk() {last_chunk.Reset();}

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
		Ptr next_chunk;
		OPtr last_chunk;

		friend struct Potato::Pointer::DefaultUniqueWrapper;
	};


	struct ComponentChunk
	{
		ComponentChunk(ComponentChunk&& chunk);
		ComponentChunk(Archetype::Ptr archetype);
		~ComponentChunk();
	protected:
		Archetype::Ptr archetype;
		Chunk::Ptr top_chunk;
		std::size_t current_component_count = 0;
		std::size_t chunk_count = 0;
	};

}