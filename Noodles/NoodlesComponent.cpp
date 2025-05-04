module;

#include <cassert>

module NoodlesComponent;

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

		for (auto& ite : target_archetype.GetMemberView())
		{
			auto mer_offset = chunk_layout.Insert(ite.struct_layout->GetLayout(suggest_component_count));
			if (!offset.has_value())
			{
				offset = mer_offset;
			}
		}

		auto final_layout = chunk_layout.Get();

		assert(final_layout.size <= suggest_size);

		auto re = Potato::IR::MemoryResourceRecord::Allocate(resource, final_layout);
		if (re)
		{
			return new(re.Get()) Chunk{ re, suggest_component_count, offset ? re.GetByte(*offset) : nullptr };
		}
		return {};
	}

	void* Chunk::GetComponent(Archetype::MemberView const& member, std::size_t entity_index)
	{
		return GetComponent(member.offset, member.layout.size, entity_index);
	}

	void* Chunk::GetComponent(std::size_t member_offset, std::size_t member_size, std::size_t entity_index)
	{
		assert(buffer != nullptr);
		if (entity_index < current_count)
		{
			return buffer + member_offset * max_count + member_size * entity_index;
		}
		return nullptr;
	}

	void* Chunk::GetComponentArray(std::size_t member_offset)
	{
		assert(buffer != nullptr);
		return buffer + member_offset * max_count;
	}

	bool Chunk::DestructEntity(Archetype const& target_archetype, std::size_t entity_index)
	{
		if(entity_index < current_count)
		{
			for (auto& ite : target_archetype)
			{
				auto buffer = GetComponent(ite, entity_index);
				auto result = ite.struct_layout->Destruction(buffer);
				assert(result);
			}
			return true;
		}
		return false;
	}

	void Chunk::MoveConstructEntity(Archetype const& target_archetype, std::size_t self_entity_index, Chunk& source_chunk, std::size_t source_entity_index)
	{
		for (auto& ite : target_archetype)
		{
			auto buffer = GetComponent(ite, self_entity_index);
			auto result = ite.struct_layout->MoveConstruction(buffer, source_chunk.GetComponent(ite, source_entity_index));
			assert(result);
		}
	}

	void Chunk::DestructAll(Archetype const& target_archetype)
	{
		if (current_count != 0)
		{
			for (auto& ite : target_archetype)
			{
				auto buffer = GetComponent(ite, 0);
				auto result = ite.struct_layout->Destruction(buffer, current_count);
				assert(result);
			}
			current_count = 0;
		}
	}

	bool Chunk::DestructComponent(Archetype const& target_archetype, std::size_t entity_index, BitFlag target_component)
	{
		auto loc = target_archetype.FindMemberIndex(target_component);
		if(loc)
		{
			auto& mem_view = target_archetype[loc];
			auto target_offset = GetComponent(mem_view, entity_index);
			auto re = mem_view.struct_layout->Destruction(target_offset);
			assert(re);
			return true;
		}
		return false;
	}

	bool Chunk::ConstructComponent(Archetype const& target_archetype, std::size_t entity_index, BitFlag target_component, void* target_buffer, bool move_construct, bool need_destruct_before_construct)
	{
		auto loc = target_archetype.FindMemberIndex(target_component);
		if (loc)
		{
			auto& mem_view = target_archetype[loc];
			auto ope = mem_view.struct_layout->GetOperateProperty();

			if(
				ope.copy_construct && !move_construct
				|| ope.move_construct && move_construct
				)
			{
				auto target_offset = GetComponent(mem_view, entity_index);
				if (need_destruct_before_construct)
				{
					auto re = mem_view.struct_layout->Destruction(target_offset);
					assert(re);
				}

				if (move_construct)
				{
					auto re = mem_view.struct_layout->MoveConstruction(target_offset, target_buffer);
					assert(re);
				}else
				{
					auto re = mem_view.struct_layout->CopyConstruction(target_offset, target_buffer);
					assert(re);
				}
				return true;
			}
		}
		return false;
	}


	void Chunk::Release()
	{
		auto re = record;
		this->~Chunk();
		re.Deallocate();
	}

	OptionalSizeT Chunk::AllocateEntityWithoutConstruct()
	{
		if (current_count < max_count)
		{
			return current_count++;
		}
		return {};
	}

	OptionalSizeT Chunk::PopEntityWithoutDestruct()
	{
		if (current_count > 0)
		{
			return current_count--;
		}
		return {};
	}

	OptionalSizeT Chunk::PopBack(Archetype const& target_archetype)
	{
		if (current_count > 0)
		{
			for (auto& ite : target_archetype)
			{
				auto buffer = GetComponent(ite, current_count - 1);
				auto result = ite.struct_layout->Destruction(buffer);
				assert(result);
			}
			--current_count;
			return current_count;
		}
		return {};
	}


	ComponentManager::ComponentManager(Config config)
		:
		archetype_info(config.resource),
		chunks(config.resource),
		archetype_resource(config.component_resource),
		component_resource(config.component_resource),
		bit_flag_container(config.resource),
		component_bitflag_container_element_count(BitFlagContainerConstViewer::GetBitFlagContainerElementCount(config.max_component_class_count))
	{

		std::size_t archtype_container_count = BitFlagContainerConstViewer::GetBitFlagContainerElementCount(config.max_archetype_count);

		max_archetype_count = BitFlagContainerConstViewer::GetMaxBitFlag(config.max_archetype_count).value;

		bit_flag_container.resize(archtype_container_count * 2);

		archetype_not_empty_bitflag = std::span(bit_flag_container).subspan(0, archtype_container_count);
		archetype_update_bitflag = std::span(bit_flag_container).subspan(archtype_container_count);
	}

	ComponentManager::~ComponentManager()
	{
		for (auto& ite : archetype_info)
		{
			auto span = ite.chunk_span.Slice(std::span(chunks));

			for (Chunk::Ptr& ite2 : span)
			{
				ite2->DestructAll(*ite.archtype);
			}
		}
		chunks.clear();
		archetype_info.clear();
	}

	OptionalSizeT ComponentManager::LocateComponentChunk(BitFlagContainerConstViewer component_bitflag) const
	{
		std::size_t index = 0;
		for (auto& ite : archetype_info)
		{
			assert(ite.archtype);
			if (ite.archtype->GetClassBitFlagContainer().IsSame(component_bitflag))
			{
				return index;
			}
			++index;
		}
		return {};
	}

	OptionalSizeT ComponentManager::CreateComponentChunk(std::span<Archetype::Init const> archetype_init)
	{
		if (archetype_info.size() < max_archetype_count)
		{
			auto old_info_size = archetype_info.size();
			auto old_chunk_size = chunks.size();

			auto archtype = Archetype::Create(component_bitflag_container_element_count, archetype_init, &archetype_resource);
			if (archtype)
			{

				ArchetypeInfo info;
				info.archtype = std::move(archtype);
				info.chunk_span = { old_chunk_size, old_chunk_size };

				archetype_info.emplace_back(std::move(info));

				BitFlag archtype_bitflag = BitFlag{ old_info_size };

				auto re = archetype_update_bitflag.SetValue(archtype_bitflag);
				assert(re.has_value());
				return old_info_size;
			}
		}
		return {};
	}

	bool ComponentManager::DestructionEntity(Index index)
	{
		if(index.archetype_index < archetype_info.size())
		{
			auto& archetype = archetype_info[index.archetype_index];
			if(index.chunk_index < archetype.chunk_span.Size())
			{
				auto& target_chunk = chunks[index.chunk_index];
				assert(target_chunk);
				return target_chunk->DestructEntity(*archetype.archtype, index.entity_index);
			}
		}
		return false;
	}

	bool ComponentManager::ConstructEntity(Index index, std::span<Init const> component_init, ConstructOption option)
	{
		if (index.archetype_index < archetype_info.size())
		{
			auto& archetype = archetype_info[index.archetype_index];

			if (index.chunk_index < archetype.chunk_span.Size())
			{
				auto& target_chunk = chunks[index.chunk_index];
				for(auto ite : component_init)
				{
					bool re = target_chunk->ConstructComponent(*archetype.archtype, index.entity_index, ite.component_class, ite.data, ite.move_construct, option.destruct_before_construct);
					assert(re);
				}
				return true;
			}
		}
		return false;
	}

	std::size_t ComponentManager::FlushInitWithComponent(Index index, BitFlagContainerConstViewer component_class, std::span<Init> in_out, std::span<Archetype::Init> in_out_archetype)
	{
		if (index.archetype_index < archetype_info.size())
		{
			auto& archetype = archetype_info[index.archetype_index];

			if (index.chunk_index < archetype.chunk_span.Size())
			{
				auto& target_chunk = chunks[index.chunk_index];
				std::size_t output_index = 0;

				if (output_index < in_out.size())
				{
					for (Archetype::MemberView const& ite : *archetype.archtype)
					{
						auto re = component_class.GetValue(ite.bitflag);
						assert(re.has_value());
						if (*re)
						{
							if (output_index < in_out_archetype.size())
							{
								auto& ref2 = in_out_archetype[output_index];
								ref2.flag = ite.bitflag;
								ref2.ptr = ite.struct_layout;
							}
							
							auto& ref = in_out[output_index];
							ref.component_class = ite.bitflag;
							ref.data = target_chunk->GetComponent(ite, index.entity_index);
							++output_index;
							if (output_index < in_out.size())
							{
								break;
							}
						}
					}
					return output_index;
				}
			}
		}
		return 0;
	}

	std::optional<ComponentManager::Index> ComponentManager::AllocateEntityWithoutConstruct(std::size_t archetype_index)
	{
		if (archetype_index < archetype_info.size())
		{
			auto& archetype = archetype_info[archetype_index];

			auto span = archetype.chunk_span.Slice(std::span(chunks));

			if (!span.empty())
			{
				auto& chunk_ref = *span.rbegin();
				auto component_index = chunk_ref->AllocateEntityWithoutConstruct();
				if (component_index)
				{
					++archetype.total_components;
					if (archetype.total_components == 1)
					{
						auto re = archetype_not_empty_bitflag.SetValue({ archetype_index });
						assert(re && !*re);
					}
					return Index{ archetype_index, archetype.chunk_span.End() - 1, component_index};
				}
			}

			auto new_chunk = Chunk::Create(*archetype.archtype, 10, &component_resource);
			if (new_chunk)
			{
				auto new_component_index = new_chunk->AllocateEntityWithoutConstruct();
				if (new_component_index)
				{
					++archetype.total_components;
					chunks.insert(chunks.begin() + archetype.chunk_span.End(), std::move(new_chunk));
					archetype.chunk_span.BackwardEnd(1);
					for (std::size_t i = archetype_index + 1; i < archetype_info.size(); ++i)
					{
						archetype_info[i].chunk_span.WholeOffset(1);
					}
					if (archetype.total_components == 1)
					{
						auto re = archetype_not_empty_bitflag.SetValue({ archetype_index });
						assert(re && !*re);
					}
					return Index{ archetype_index, archetype.chunk_span.End() - 1, new_component_index };
				}
			}
		}
		return {};
	}

	std::optional<bool> ComponentManager::PopBackEntityToFillHole(Index hole_index, Index max_hole)
	{
		assert(hole_index.archetype_index == max_hole.archetype_index && hole_index <= max_hole);
		if (hole_index.archetype_index < archetype_info.size())
		{
			auto& archetype = archetype_info[hole_index.archetype_index];

			if (hole_index.chunk_index < archetype.chunk_span.Size())
			{
				std::span<Chunk::Ptr> span = archetype.chunk_span.Slice(std::span(chunks));
				auto index_end = Index{ hole_index.archetype_index, span.size() - 1, (*span.rbegin())->GetEntityCount() - 1};
				
				OptionalSizeT PopBackResult;
				std::optional<bool> result;
				if (index_end == max_hole)
				{
					PopBackResult = (*span.rbegin())->PopBack(*archetype.archtype);
					result = false;
				}
				else {

					auto& tar = span[hole_index.chunk_index];
					auto& sour = (*span.rbegin());

					auto re = tar->DestructEntity(*archetype.archtype, hole_index.entity_index);
					assert(re);
					tar->MoveConstructEntity(*archetype.archtype, hole_index.entity_index, *sour, sour->GetEntityCount() - 1);
					sour->PopBack(*archetype.archtype);
					result = true;
				}

				--archetype.total_components;

				if (archetype.total_components == 1)
				{
					auto re = archetype_not_empty_bitflag.SetValue({ hole_index.archetype_index }, false);
					assert(re && *re);
				}

				if ((*span.rbegin())->GetEntityCount() == 0)
				{
					chunks.erase(chunks.begin() + archetype.chunk_span.End() - 1);
					archetype.chunk_span.SubIndex(0, archetype.chunk_span.Size() - 1);
					for (std::size_t i = hole_index.archetype_index + 1; i < archetype_info.size(); ++i)
					{
						archetype_info[i].chunk_span.WholeForward(1);
					}
				}
				return result;
			}

		}
		return std::nullopt;
	}

	void ComponentManager::ClearBitFlag()
	{
		archetype_update_bitflag.Reset();
	}

	bool ComponentManager::IsArchetypeAcceptQuery(std::size_t archetype_index, BitFlagContainerConstViewer query_class, BitFlagContainerConstViewer refuse_qurey_class) const
	{
		if (archetype_index < archetype_info.size())
		{
			auto& archetype = archetype_info[archetype_index];
			auto container = archetype.archtype->GetClassBitFlagContainer();
			auto re = container.Inclusion(query_class);
			if (re.has_value() && *re)
			{
				re = container.Inclusion(refuse_qurey_class);
				if (!re.has_value() || !*re || refuse_qurey_class.IsReset())
				{
					return true;
				}
			}
		}
		return false;
	}

	bool ComponentManager::TranslateClassToQueryData(std::size_t archetype_index, std::span<BitFlag const> target_class, std::span<std::size_t> outoput_query_data) const
	{
		if (archetype_index < archetype_info.size() && outoput_query_data.size() >= target_class.size() * 2)
		{
			auto& archetype = archetype_info[archetype_index];

			for (auto ite : target_class)
			{
				auto loc = archetype.archtype->FindMemberIndex(ite);
				if (loc)
				{
					auto& mm = (*archetype.archtype)[loc.Get()];
					outoput_query_data[0] = mm.offset;
					outoput_query_data[1] = mm.layout.size;
					outoput_query_data = outoput_query_data.subspan(2);
				}
				else {
					return false;
				}
			}
			return true;
		}
		return false;
	}

	OptionalSizeT ComponentManager::QueryComponentArray(std::size_t archetype_index, std::size_t chunk_index, std::span<std::size_t const> query_data, std::span<void*> output) const
	{
		if (archetype_index < archetype_info.size() && query_data.size() <= output.size() * 2)
		{
			assert((query_data.size() % GetQueryDataCount()) == 0);
			auto& archetype = archetype_info[archetype_index];
			if (chunk_index < archetype.chunk_span.Size())
			{
				auto& chunk = chunks[archetype.chunk_span.Begin() + chunk_index];
				while (!query_data.empty())
				{
					output[0] = chunk->GetComponentArray(query_data[0]);
					query_data = query_data.subspan(2);
					output = output.subspan(1);
				}
				return chunk->GetEntityCount();
			}
		}
		return {};
	}

	bool ComponentManager::QueryComponent(Index index, std::span<std::size_t const> query_data, std::span<void*> output) const
	{
		if (index.archetype_index < archetype_info.size() && query_data.size() <= output.size() * 2)
		{
			assert((query_data.size() % 2) == 0);
			auto& archetype = archetype_info[index.archetype_index];
			if (index.chunk_index < archetype.chunk_span.Size())
			{
				auto& chunk = chunks[archetype.chunk_span.Begin() + index.chunk_index];
				if (index.entity_index < chunk->GetEntityCount())
				{
					while (!query_data.empty())
					{
						output[0] = chunk->GetComponent(query_data[0], query_data[1], index.entity_index);
						query_data = query_data.subspan(2);
						output = output.subspan(1);
					}
				}
				return true;
			}
		}
		return {};
	}

	void ComponentManager::Sort(std::span<Archetype::Init> init_list)
	{
		std::sort(init_list.begin(), init_list.end(), [](Archetype::Init const& i1, Archetype::Init const& i2) {
			auto i1_layout = i1.ptr->GetLayout();
			auto i2_layout = i2.ptr->GetLayout();
			auto re = i1_layout <=> i2_layout;
			if (re == std::strong_ordering::equal)
			{
				return ((i1.ptr->GetHashCode()) <=> (i2.ptr->GetHashCode())) == std::strong_ordering::less;
			}
			return re == std::strong_ordering::less;
		});
	}

}