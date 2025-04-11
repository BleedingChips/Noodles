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

	std::byte* Chunk::GetComponent(Archetype::MemberView const& member, std::size_t component_index)
	{
		assert(component_index < current_count);
		assert(buffer != nullptr);
		return buffer + member.offset * max_count + member.layout.size * component_index;
	}

	void Chunk::DestructComponent(Archetype const& target_archetype, std::size_t component_index)
	{
		for (auto& ite : target_archetype)
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

	void Chunk::Release()
	{
		auto re = record;
		this->~Chunk();
		re.Deallocate();
	}

	OptionalSizeT Chunk::AllocateComponentWithoutConstruct()
	{
		if (current_count < max_count)
		{
			return current_count++;
		}
		return {};
	}

	OptionalSizeT Chunk::PopComponentWithoutDestruct()
	{
		if (current_count > 0)
		{
			return current_count--;
		}
		return {};
	}


	ComponentChunkManager::ComponentChunkManager(GlobalContext::Ptr global_context, Config config)
		:
		component_chunks_info(config.resource),
		chunks(config.resource),
		archetype_resource(config.component_resource),
		component_resource(config.component_resource),
		global_context(std::move(global_context)),
		bit_flag_container(config.resource)
	{
		assert(this->global_context);

		std::size_t archtype_container_count = BitFlagConstContainer::GetBitFlagContainerElementCount(config.max_archetype_count);

		max_archtype_count = BitFlagConstContainer::GetMaxBitFlag(config.max_archetype_count).value;

		bit_flag_container.resize(archtype_container_count * 2);

		component_chunk_not_empty_bitflag = std::span(bit_flag_container).subspan(0, archtype_container_count);
		component_chunk_update_bitflag = std::span(bit_flag_container).subspan(archtype_container_count);

		/*
		archetype_mask.resize(manager.GetArchetypeStorageCount() * 2);
		this->manager = &manager;
		auto temp_span = std::span(archetype_mask);
		archetype_usage_mask = temp_span.subspan(0, manager.GetArchetypeStorageCount());
		archetype_update_mask = temp_span.subspan(manager.GetArchetypeStorageCount());
		*/
	}

	ComponentChunkManager::~ComponentChunkManager()
	{
		for (auto& ite : component_chunks_info)
		{
			auto span = ite.chunk_span.Slice(std::span(chunks));

			for (Chunk::Ptr& ite2 : span)
			{
				ite2->DestructAllComponent(*ite.archtype);
			}
		}
		chunks.clear();
		component_chunks_info.clear();
	}

	OptionalSizeT ComponentChunkManager::LocateComponentChunk(BitFlagConstContainer component_bitflag) const
	{
		std::size_t index = 0;
		for (auto& ite : component_chunks_info)
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

	/*
	auto ComponentChunkManager::GetComponentChunk(std::size_t component_chunk_index) const -> ComponentChunkOPtr
	{
		if (component_chunk_index < component_chunks.size())
		{
			auto result = &component_chunks[component_chunk_index];
		}
		return {};
	}
	*/

	OptionalSizeT ComponentChunkManager::CreateComponentChunk(std::span<Archetype::Init const> archetype_init)
	{
		auto old_info_size = component_chunks_info.size();
		auto old_chunk_size = chunks.size();

		auto archtype = Archetype::Create(global_context->GetComponentBitFlagContainerElementCount(), archetype_init, &archetype_resource);
		if (archtype)
		{

			ChunkInfo info;
			info.archtype = std::move(archtype);
			info.chunk_span = { old_chunk_size, old_chunk_size };

			component_chunks_info.emplace_back(std::move(info));
			return old_info_size;
		}
		return {};
	}



	/*
	std::tuple<std::size_t, std::size_t> CalculateChunkSize(Potato::IR::Layout archetype_layout, std::size_t min_count, std::size_t min_size)
	{
		std::size_t suggest_size = min_size;
		std::size_t suggest_count = suggest_size / archetype_layout.size;
		while(suggest_count < min_count)
		{
			if(suggest_size < component_page_huge_multiply_max_size)
			{
				suggest_size *= component_page_huge_multiply;
			}else
			{
				suggest_size *= 2;
			}
			suggest_count = suggest_size / archetype_layout.size;
		}
		return { suggest_size, suggest_count };
	}


	std::byte* ChunkView::GetComponent(
		Archetype::MemberView const& view,
		std::size_t column_index
	) const
	{
		if(buffer != nullptr)
		{
			return
				buffer +
				view.offset * max_count + view.layout->GetLayout().size * column_index;
		}
		return nullptr;
	}

	std::byte* ChunkView::GetComponent(
		MarkIndex index,
		std::size_t column_index
	) const
	{
		auto mindex = archetype->Locate(index);
		if(mindex.has_value())
		{
			return GetComponent(*mindex, column_index);
		}
		return nullptr;
	}

	std::byte* ChunkView::GetComponent(
		Archetype::Index index,
		std::size_t column_index
	) const
	{
		auto& mm = archetype->GetMemberView(index);
		return GetComponent(mm, column_index);
	}

	bool ChunkView::MoveConstructComponent(
		MarkIndex index,
		std::size_t column_index,
		std::byte* target_buffer
	) const
	{
		auto mindex = archetype->Locate(index);
		if(mindex.has_value())
		{
			auto& mm = archetype->GetMemberView(*mindex);
			auto target = GetComponent(mm, column_index);
			auto re = mm.layout->MoveConstruction(
				target, target_buffer
			);
			assert(re);
			return true;
		}
		return false;
	}

	ChunkInfo::ChunkInfo(ChunkInfo&& other)
		: archetype(std::move(other.archetype)), record(other.record), current_count(other.current_count), max_count(other.max_count), column_size(other.column_size)
	{
		other.archetype.Reset();
		other.record = {};
		other.current_count = 0;
		other.max_count = 0;
	}

	ChunkInfo::~ChunkInfo()
	{
		if(record)
		{
			for(auto& ite : archetype->GetMemberView())
			{
				ite.layout->Destruction(
					record.GetByte() + ite.offset * ite.layout->GetLayout().size,
					current_count
				);
			}
			record.Deallocate();
			archetype.Reset();
			record = {};
			current_count = 0;
			max_count = 0;
			column_size = 0;
		}
	}

	

	bool ComponentManager::ReadComponentRow_AssumedLocked(ComponentQuery const& query, std::size_t filter_ite, QueryData& accessor) const
	{
		std::shared_lock sl(query.GetMutex());
		std::size_t archetype_index = 0;
		if(query.VersionCheck_AssumedLocked(reinterpret_cast<std::size_t>(this), chunk_infos.size()))
		{
			auto re = query.EnumMountPointIndexByIterator_AssumedLocked(filter_ite, archetype_index);
			if (re.has_value() && re->size() <= accessor.output_buffer.size())
			{
				assert(chunk_infos.size() > archetype_index);
				auto& ref = chunk_infos[archetype_index];
				auto view = ref.GetView();
				if (view)
				{
					for (std::size_t i = 0; i < re->size(); ++i)
					{
						accessor.output_buffer[i] = view.GetComponent(
							Archetype::Index{ (*re)[i] },
							0
						);
					}
					accessor.archetype = view.archetype;
					accessor.array_size = view.current_count;
					return true;
				}
			}
		}
		return false;
	}

	bool ComponentManager::ReadComponentRow_AssumedLocked(std::size_t archetype_index, ComponentQuery const& query, QueryData& accessor) const
	{
		std::shared_lock sl(query.GetMutex());
		if (CheckVersion_AssumedLocked(query))
		{
			auto re = query.EnumMountPointIndexByArchetypeIndex_AssumedLocked(archetype_index);
			if (re.has_value() && re->size() <= accessor.output_buffer.size())
			{
				assert(chunk_infos.size() > archetype_index);
				auto& ref = chunk_infos[archetype_index];
				auto view = ref.GetView();
				if (view)
				{
					for (std::size_t i = 0; i < re->size(); ++i)
					{
						accessor.output_buffer[i] = view.GetComponent(
							Archetype::Index{ (*re)[i] },
							0
						);
					}
					accessor.archetype = view.archetype;
					accessor.array_size = view.current_count;
					return true;
				}
			}
		}
		return false;
	}

	bool ComponentManager::CheckVersion_AssumedLocked(ComponentQuery const& query) const
	{
		return query.VersionCheck_AssumedLocked(reinterpret_cast<std::size_t>(this));
	}

	bool ComponentManager::ReadComponent_AssumedLocked(std::size_t archetype_index, std::size_t column_index, ComponentQuery const& query, QueryData& accessor) const
	{
		std::shared_lock sl(query.GetMutex());
		if (CheckVersion_AssumedLocked(query))
		{
			auto re = query.EnumMountPointIndexByArchetypeIndex_AssumedLocked(archetype_index);
			if (re.has_value() && re->size() <= accessor.output_buffer.size())
			{
				auto view = GetChunk_AssumedLocked(archetype_index);
				if (view)
				{
					for (std::size_t i = 0; i < re->size(); ++i)
					{
						accessor.output_buffer[i] = view.GetComponent(
							Archetype::Index{ (*re)[i] },
							column_index
						);
					}
					accessor.archetype = view.archetype;
					accessor.array_size = 1;
					return true;
				}
			}
		}
		return false;
	}

	bool ComponentManager::UpdateFilter_AssumedLocked(ComponentQuery& query) const
	{
		std::lock_guard lg(query.GetMutex());

		auto re = query.Update_AssumedLocked(*manager, reinterpret_cast<std::size_t>(this), chunk_infos.size());

		if(re.has_value())
		{
			for (std::size_t i = *re; i < chunk_infos.size(); ++i)
			{
				query.OnCreatedArchetype_AssumedLocked(i, *chunk_infos[i].archetype);
			}
			return true;
		}
		return false;
	}

	ComponentManager::ComponentManager(StructLayoutManager& manager, Config config)
		:
		archetype_resource(config.archetype_resource),
		component_resource(config.component_resource),
		archetype_mask(config.resource)
	{
		archetype_mask.resize(manager.GetArchetypeStorageCount() * 2);
		this->manager = &manager;
	}

	
	

	std::tuple<Archetype::OPtr, OptionalIndex> ComponentManager::FindArchetype_AssumedLocked(std::span<MarkElement const> require_archetype) const
	{
		for(std::size_t i = 0; i < chunk_infos.size(); ++i)
		{
			auto& ite = chunk_infos[i];
			if (MarkElement::IsSame(require_archetype, ite.archetype->GetAtomicTypeMark()))
			{
				return {ite.archetype, i};
			}
		}
		return {{}, {}};
	}

	ComponentManager::ArchetypeBuilderRef::ArchetypeBuilderRef(ComponentManager& manager, std::pmr::memory_resource* temp_resource)
		: atomic_type(temp_resource), mark(temp_resource)
	{
		mark.resize(manager.manager->GetComponentStorageCount());
	}

	bool ComponentManager::ArchetypeBuilderRef::Insert(StructLayout::Ptr ref, MarkIndex index)
	{
		auto re = MarkElement::Mark(mark, index);
		if (!re)
		{
			auto layout = ref->GetLayout();
			auto find = std::find_if(
				atomic_type.begin(),
				atomic_type.end(),
				[=](Archetype::Init const& ite)
				{
					auto cur_layout = ite.ptr->GetLayout();
					return cur_layout.align < layout.align || cur_layout.size < layout.size;
				}
			);
			atomic_type.insert(find, {std::move(ref), index });
			return true;
		}
		return false;
	}

	void ComponentManager::ArchetypeBuilderRef::Clear()
	{
		atomic_type.clear();
		MarkElement::Reset(mark);
	}

	std::tuple<Archetype::OPtr, OptionalIndex> ComponentManager::CreateArchetype_AssumedLocked(ArchetypeBuilderRef const& ref)
	{
		if(chunk_infos.size() + 1 >= manager->GetArchetypeCount())
			return {{}, {}};


		auto ptr = Archetype::Create(
			manager->GetComponentStorageCount(), ref.atomic_type, &archetype_resource
		);

		assert(ptr);
		auto old_size = chunk_infos.size();

		ChunkInfo infos;
		infos.archetype = ptr;
		
		chunk_infos.emplace_back(
			std::move(infos)
		);

		return {ptr, old_size};
	}

	bool ComponentManager::ReleaseComponentColumn_AssumedLocked(std::size_t archetype_index, std::size_t column_index, std::pmr::vector<RemovedColumn>& removed)
	{
		auto mm = GetChunk_AssumedLocked(archetype_index);
		if(mm.current_count > column_index)
		{
			for(auto& ite : mm.archetype->GetMemberView())
			{
				auto tar = mm.GetComponent(ite, column_index);
				ite.layout->Destruction(tar);
			}
			removed.emplace_back(
				archetype_index,
				column_index
			);
			return true;
		}
		return false;
	}

	ChunkView ComponentManager::GetChunk_AssumedLocked(std::size_t archetype_index) const
	{
		assert(archetype_index < chunk_infos.size());
		return chunk_infos[archetype_index].GetView();
	}

	OptionalIndex ComponentManager::AllocateComponentColumn_AssumedLocked(std::size_t archetype_index, std::pmr::vector<RemovedColumn>& removed_list)
	{
		if(archetype_index < chunk_infos.size())
		{
			auto find = std::find_if(
				removed_list.begin(),
				removed_list.end(),
				[=](RemovedColumn const& col)
				{
					return col.archetype_index == archetype_index;
				}
			);
			if(find != removed_list.end())
			{
				auto result = find->column_index;
				removed_list.erase(find);
				return result;
			}else
			{
				auto& ref = chunk_infos[archetype_index];
				if(ref.record.GetByte() == nullptr)
				{
					auto archetype_layout = ref.archetype->GetArchetypeLayout();
					auto [size, count] = CalculateChunkSize(archetype_layout, component_page_min_columns_count, component_page_min_size);
					auto total_layout = Potato::MemLayout::MemLayoutCPP{ archetype_layout.WithArray(count)};
					auto final_layout = total_layout.Get();
					assert(final_layout.size <= size);
					auto re = Potato::IR::MemoryResourceRecord::Allocate(&component_resource, final_layout);
					if(re)
					{
						ref.record = re;
						ref.max_count = count;
						ref.column_size = archetype_layout.size;
						ref.current_count = 1;
						MarkElement::Mark(GetWriteableArchetypeUsageMark_AssumedLocked(), MarkIndex{ archetype_index });
						MarkElement::Mark(GetWriteableArchetypeUpdateMark_AssumedLocked(), MarkIndex{ archetype_index });
						return 0;
					}
				}else
				{
					ref.current_count += 1;
					if(ref.current_count < ref.max_count)
					{
						if (ref.current_count == 1)
						{
							MarkElement::Mark(GetWriteableArchetypeUpdateMark_AssumedLocked(), MarkIndex{ archetype_index });
						}
						return ref.current_count - 1;
					}else
					{
						assert(false);
						ref.current_count -= 1;
						return {};
					}
				}
			}
		}
		return {};
	}

	void ComponentManager::FixComponentChunkHole_AssumedLocked(std::pmr::vector<RemovedColumn>& holes, void(*func)(void* data, ChunkView const& view, std::size_t, std::size_t), void* data)
	{
		std::sort(
			holes.begin(),
			holes.end(),
			[](RemovedColumn c1, RemovedColumn c2)
			{
				auto order = Potato::Misc::PriorityCompareStrongOrdering(
					c1.archetype_index, c2.archetype_index,
					c1.column_index, c2.column_index
				);
				return order == std::strong_ordering::greater;
			}
		);
		for(auto& ite : holes)
		{
			auto& chunk = chunk_infos[ite.archetype_index];
			if(ite.column_index + 1 == chunk.current_count)
			{
				chunk.current_count -= 1;
				if(chunk.current_count == 0)
				{
					chunk.record.Deallocate();
					auto re = MarkElement::Mark(GetWriteableArchetypeUsageMark_AssumedLocked(), MarkIndex{ite.archetype_index}, false);
					assert(re);
					re = MarkElement::Mark(GetWriteableArchetypeUpdateMark_AssumedLocked(), MarkIndex{ ite.archetype_index }, true);
				}
			}else
			{
				assert(ite.column_index + 1 < chunk.current_count);
				auto mm = chunk.GetView();
				chunk.current_count -= 1;
				for(auto& ite2 : mm.archetype->GetMemberView())
				{
					auto source = mm.GetComponent(ite2, chunk.current_count);
					auto target = mm.GetComponent(ite2, ite.column_index);
					auto re1 = ite2.layout->MoveConstruction(target, source);
					auto re2 = ite2.layout->Destruction(source);
					assert(re1);
					assert(re2);
				}

				if(func != nullptr)
				{
					(*func)(data, mm, chunk.current_count, ite.column_index);
				}
			}
		}
		holes.clear();
	}
	*/
}