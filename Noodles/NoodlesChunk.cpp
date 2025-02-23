module;

#include <cassert>

module NoodlesChunk;
import PotatoMemLayout;

constexpr std::size_t manager_size = 10;

constexpr std::size_t component_page_min_columns_count = 10;
constexpr std::size_t component_page_min_size = 4096;

constexpr std::size_t component_page_huge_multiply_max_size = component_page_min_size * 32;
constexpr std::size_t component_page_huge_multiply = 4;

namespace Noodles
{

	Chunk::Ptr Chunk::Create(Archetype const& target_archetype, std::size_t suggest_component_count, std::pmr::memory_resource* resource)
	{
		auto archetype_layout = target_archetype.GetArchetypeLayout();

		std::size_t suggest_size = component_page_min_size;
		suggest_component_count = std::max(suggest_component_count, component_page_min_columns_count);

		{
			auto chunk_layout = Potato::MemLayout::MemLayoutCPP::Get<Chunk>();
			auto offset = chunk_layout.Insert(target_archetype.GetLayout());

			while (true)
			{
				suggest_component_count = (suggest_size - offset) / archetype_layout.size;
				if (suggest_component_count > component_page_min_columns_count)
				{
					break;
				}
				else
				{
					if (suggest_size < component_page_huge_multiply_max_size)
					{
						suggest_size *= component_page_huge_multiply;
					}
					else
					{
						suggest_size *= 2;
					}
				}
			}
		}

		auto chunk_layout = Potato::MemLayout::MemLayoutCPP::Get<Chunk>();
		std::optional<std::size_t> offset;

		for(auto& ite : target_archetype)
		{
			auto mer_offset = chunk_layout.Insert(ite.struct_layout->GetLayout(suggest_component_count));
			if(!offset.has_value())
			{
				offset = mer_offset;
			}
		}

		auto final_layout = chunk_layout.Get();

		assert(final_layout.size <= suggest_size);

		auto re = Potato::IR::MemoryResourceRecord::Allocate(resource, final_layout);
		if (re)
		{
			return new(re.Get()) Chunk{re, suggest_component_count, offset ? re.GetByte(*offset) : nullptr};
		}
		return {};
	}

	std::byte* Chunk::GetComponent(Archetype::MemberView const& member, std::size_t component_index)
	{
		assert(component_index < current_count);
		assert(buffer != nullptr);
		return buffer + member.offset * max_count + member.layout.size * component_index;
	}

	void Chunk::DestructComponent(Archetype const& target_archetype, std::size_t component_index)
	{
		for(auto& ite : target_archetype)
		{
			auto buffer = GetComponent(ite, component_index);
			auto result = ite.struct_layout->Destruction(buffer);
			assert(result);
		}
	}

	void Chunk::MoveConstructComponent(Archetype const& target_archetype, std::size_t self_component_index, Chunk& source_chunk, std::size_t source_component_index)
	{
		for (auto& ite : target_archetype)
		{
			auto buffer = GetComponent(ite, self_component_index);
			auto result = ite.struct_layout->MoveConstruction(buffer, source_chunk.GetComponent(ite, source_component_index));
			assert(result);
		}
	}

	void Chunk::DestructAllComponent(Archetype const& target_archetype)
	{
		for (auto& ite : target_archetype)
		{
			auto buffer = GetComponent(ite, 0);
			auto result = ite.struct_layout->Destruction(buffer, current_count);
			assert(result);
		}
	}

	void Chunk::Release()
	{
		auto re = record;
		this->~Chunk();
		re.Deallocate();
	}

	OptionalIndex Chunk::AllocateComponentWithoutConstruct()
	{
		if(current_count < max_count)
		{
			return current_count++;
		}
		return {};
	}

	OptionalIndex Chunk::PopComponentWithoutDestruct()
	{
		if(current_count > 0)
		{
			return current_count--;
		}
		return {};
	}

	ComponentChunk::ComponentChunk(ComponentChunk&& chunk)
		: archetype(std::move(chunk.archetype)), top_chunk(std::move(chunk.top_chunk)), current_component_count(chunk.current_component_count), chunk_count(chunk.chunk_count)
	{
		chunk.current_component_count = 0;
		chunk.chunk_count = 0;
	}

	ComponentChunk::ComponentChunk(Archetype::Ptr archetype)
		: archetype(archetype)
	{
		
	}

	ComponentChunk::~ComponentChunk()
	{
		if(top_chunk)
		{
			assert(archetype);
			Chunk::OPtr cur = top_chunk;
			while(cur)
			{
				cur->DestructAllComponent(*archetype);
				cur->ReleaseLastChunk();
				cur = cur->MoveNextChunk();
			}
			current_component_count = 0;
			chunk_count = 0;
		}
		assert(current_component_count == 0 && chunk_count == 0);
	}
}