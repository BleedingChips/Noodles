module;

#include <cassert>

module NoodlesComponent;
import PotatoMemLayout;

constexpr std::size_t manager_size = 10;

constexpr std::size_t component_page_min_columns_count = 10;
constexpr std::size_t component_page_min_size = 4096;

constexpr std::size_t component_page_huge_multiply_max_size = component_page_min_size * 32;
constexpr std::size_t component_page_huge_multiply = 4;

namespace Noodles
{

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

	ComponentFilter::Ptr ComponentFilter::Create(
		StructLayoutMarkIndexManager& manager,
		std::size_t archetype_storage_count,
		std::span<StructLayoutWriteProperty const> require_component_type,
		std::span<StructLayout::Ptr const> refuse_component_type,
		std::pmr::memory_resource* storage_resource,
		std::pmr::memory_resource* archetype_info_resource
	)
	{
		auto layout = Potato::MemLayout::MemLayoutCPP::Get<ComponentFilter>();
		auto require_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<MarkElement>(manager.GetStorageCount()));
		auto require_write_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<MarkElement>(manager.GetStorageCount()));
		auto refuse_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<MarkElement>(manager.GetStorageCount()));
		auto archetype_usable_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<MarkElement>(archetype_storage_count));
		auto atomic_index_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<MarkIndex>(require_component_type.size()));
		auto re = Potato::IR::MemoryResourceRecord::Allocate(storage_resource, layout.Get());
		if(re)
		{
			std::span<MarkElement> require_mask { new (re.GetByte(require_offset)) MarkElement[manager.GetStorageCount()], manager.GetStorageCount() };
			std::span<MarkElement> require_write_mask{ new (re.GetByte(require_write_offset)) MarkElement[manager.GetStorageCount()], manager.GetStorageCount() };
			std::span<MarkElement> archetype_usable{ new (re.GetByte(archetype_usable_offset)) MarkElement[archetype_storage_count], archetype_storage_count };
			std::span<MarkIndex> require_index { new (re.GetByte(atomic_index_offset)) MarkIndex[require_component_type.size()], require_component_type.size() };

			auto ite_span = require_index;

			for(auto& Ite : require_component_type)
			{
				auto loc = manager.LocateOrAdd(Ite.struct_layout);
				if(loc.has_value())
				{
					MarkElement::Mark(require_mask, *loc);
					ite_span[0] = *loc;
					ite_span = ite_span.subspan(1);
					if(Ite.need_write)
					{
						MarkElement::Mark(require_write_mask, *loc);
					}
				}else
				{
					assert(false);
				}
			}

			std::span<MarkElement> refuse_mask{ new (re.GetByte(refuse_offset)) MarkElement[manager.GetStorageCount()], manager.GetStorageCount() };

			for(auto& Ite : refuse_component_type)
			{
				auto loc = manager.LocateOrAdd(Ite);
				if (loc.has_value())
				{
					MarkElement::Mark(refuse_mask, *loc);
				}
				else
				{
					assert(false);
				}
			}

			return new (re.Get()) ComponentFilter { re, require_mask, require_write_mask, refuse_mask, archetype_usable, require_index, archetype_info_resource };
 		}
		return {};
	}

	bool ComponentFilter::OnCreatedArchetype(std::size_t archetype_index, Archetype const& archetype)
	{
		std::lock_guard lg(mutex);
		auto archetype_atomic_id = archetype.GetAtomicTypeMark();
		if(MarkElement::Inclusion(archetype_atomic_id, GetRequiredStructLayoutMarks().total_marks) && !MarkElement::IsOverlapping(archetype_atomic_id, refuse_component))
		{
			auto old_size = archetype_member.size();
			auto atomic_span = GetMarkIndex();
			auto atomic_size = atomic_span.size();
			archetype_member.resize(old_size + 1 + atomic_size);
			auto span = std::span(archetype_member).subspan(old_size);
			span[0] = archetype_index;
			for(std::size_t i = 0; i < atomic_size; ++i)
			{
				auto index = archetype.Locate(atomic_span[i]);
				assert(index.has_value());
				span[i + 1] = index->index;
			}
			auto re = MarkElement::Mark(archetype_usable, MarkIndex{ archetype_index });
			assert(!re);
			return true;
		}
		return false;
	}

	std::optional<std::span<std::size_t const>> ComponentFilter::EnumMountPointIndexByArchetypeIndex_AssumedLocked(std::size_t archetype_index) const
	{
		auto size = GetMarkIndex().size();
		auto span = std::span(archetype_member);
		assert((span.size() % (size + 1)) == 0);
		while (!span.empty())
		{
			if (span[0] == archetype_index)
			{
				span = span.subspan(1, size);
				return span;
			}
			else
			{
				span = span.subspan(size + 1);
			}
		}
		return std::nullopt;
	}

	std::optional<std::span<std::size_t const>> ComponentFilter::EnumMountPointIndexByIterator_AssumedLocked(std::size_t ite_index, std::size_t& archetype_index) const
	{
		auto size = GetMarkIndex().size();
		auto span = std::span(archetype_member);
		assert((span.size() % (size + 1)) == 0);

		auto offset = ite_index * (size + 1);
		if (offset < span.size())
		{
			span = span.subspan(offset);
			archetype_index = span[0];
			span = span.subspan(1, size);
			return span;
		}
		
		return std::nullopt;
	}

	bool ComponentFilter::IsIsOverlappingRunTime(ComponentFilter const& other, std::span<MarkElement const> archetype_usage) const
	{
		std::shared_lock sl(mutex);
		return
			MarkElement::IsOverlappingWithMask(archetype_usable, other.archetype_usable, archetype_usage) && (
			MarkElement::IsOverlapping(require_component, other.require_write_component)
			|| MarkElement::IsOverlapping(require_write_component, other.require_component)
			);
	}

	auto ComponentManager::ReadComponentRow_AssumedLocked(ComponentFilter const& filter, std::size_t filter_ite, std::pmr::memory_resource* wrapper_resource) const
		->std::optional<ComponentRowWrapper>
	{
		std::shared_lock sl(filter.mutex);
		std::size_t archetype_index = 0;
		auto re = filter.EnumMountPointIndexByIterator_AssumedLocked(filter_ite, archetype_index);
		if(re.has_value())
		{
			assert(chunk_infos.size() > archetype_index);
			auto& ref = chunk_infos[archetype_index];
			auto view = ref.GetView();
			if(view)
			{
				std::pmr::vector<void*> temp_elements{ wrapper_resource };
				temp_elements.resize(re->size());
				for (std::size_t i = 0; i < temp_elements.size(); ++i)
				{
					temp_elements[i] = view.GetComponent(
						Archetype::Index{(*re)[i]},
						0
					);
				}
				return ComponentRowWrapper
				{
					view.archetype,
					
					std::move(temp_elements),
					view.current_count
				};
			}
			ComponentRowWrapper wrapper
			{
				std::move(view.archetype),
				{},
				0
			};
			return wrapper;
		}
		return std::nullopt;
	}

	auto ComponentManager::ReadComponentRow_AssumedLocked(std::size_t archetype_index, ComponentFilter const& filter, std::pmr::memory_resource* wrapper_resource) const
		->std::optional<ComponentRowWrapper>
	{
		std::shared_lock sl(filter.mutex);
		auto re = filter.EnumMountPointIndexByArchetypeIndex_AssumedLocked(archetype_index);
		if (re.has_value())
		{
			assert(chunk_infos.size() > archetype_index);
			auto& ref = chunk_infos[archetype_index];
			auto view = ref.GetView();
			if (view)
			{
				std::pmr::vector<void*> temp_elements{ wrapper_resource };
				temp_elements.resize(re->size());
				for (std::size_t i = 0; i < temp_elements.size(); ++i)
				{
					temp_elements[i] = view.GetComponent(
						Archetype::Index{ (*re)[i] },
						0
					);
				}
				return ComponentRowWrapper
				{
					view.archetype,
					std::move(temp_elements),
					view.current_count
					
				};
			}else
			{
				ComponentRowWrapper wrapper
				{
					std::move(view.archetype),
					{},
					0
				};
				return wrapper;
			}
		}
		return std::nullopt;
	}

	auto ComponentManager::ReadComponent_AssumedLocked(std::size_t archetype_index, std::size_t column_index, ComponentFilter const& filter, std::pmr::memory_resource* wrapper_resource) const
		->std::optional<ComponentRowWrapper>
	{
		std::shared_lock sl(filter.mutex);
		auto re = filter.EnumMountPointIndexByArchetypeIndex_AssumedLocked(archetype_index);
		if (re.has_value())
		{
			auto view = GetChunk_AssumedLocked(archetype_index);
			if(view)
			{
				if (view)
				{
					std::pmr::vector<void*> temp_elements{ wrapper_resource };
					temp_elements.resize(re->size());
					for (std::size_t i = 0; i < temp_elements.size(); ++i)
					{
						temp_elements[i] = view.GetComponent(
							Archetype::Index{ (*re)[i] },
							column_index
						);
					}
					return ComponentRowWrapper
					{
						view.archetype,
						std::move(temp_elements),
						1
					};
				}
				else
				{
					ComponentRowWrapper wrapper
					{
						std::move(view.archetype),
						{},
						0
					};
					return wrapper;
				}
			}
		}
		return std::nullopt;
	}

	ComponentFilter::Ptr ComponentManager::CreateComponentFilter(
		std::span<StructLayoutWriteProperty const> require_component,
		std::span<StructLayout::Ptr const> refuse_component,
		std::size_t identity,
		std::pmr::memory_resource* filter_resource,
		std::pmr::memory_resource* offset_resource
	)
	{
		auto filter = ComponentFilter::Create(
			manager,
			archetype_storage_count,
			require_component,
			refuse_component,
			filter_resource,
			offset_resource
		);
		if(filter)
		{
			{
				std::shared_lock sl(chunk_mutex);
				for(std::size_t i = 0; i < chunk_infos.size(); ++i)
				{
					filter->OnCreatedArchetype(i, *chunk_infos[i].archetype);
				}
			}

			std::lock_guard lg(filter_mutex);
			filters.emplace_back(
				filter,
				OptionalIndex{identity}
			);
			return filter;
		}
		return {};
	}

	ComponentManager::ComponentManager(Config config)
		:
		archetype_max_count(MarkElement::GetMaxMarkIndexCount(config.archetype_max_count)),
		archetype_storage_count(MarkElement::GetMarkElementStorageCalculate(config.archetype_max_count)),
		manager(config.component_max_count, config.resource),
		archetype_resource(config.archetype_resource),
		component_resource(config.component_resource),
		archetype_mask(config.resource),
		filters(config.resource)
	{
		archetype_mask.resize(archetype_storage_count * 2);
	}

	
	ComponentManager::~ComponentManager()
	{
		{
			std::lock_guard lg2(chunk_mutex);
			chunk_infos.clear();
		}

		{
			std::lock_guard lg(filter_mutex);
			filters.clear();
		}
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
		mark.resize(manager.GetComponentMarkElementStorageCount());
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
		if(chunk_infos.size() + 1 >= archetype_max_count)
			return {{}, {}};


		auto ptr = Archetype::Create(
			manager, ref.atomic_type, &archetype_resource
		);
		assert(ptr);
		auto old_size = chunk_infos.size();

		ChunkInfo infos;
		infos.archetype = ptr;
		
		chunk_infos.emplace_back(
			std::move(infos)
		);

		{
			std::shared_lock sl(filter_mutex);
			for (auto& ite : filters)
			{
				if (ite.identity)
				{
					ite.filter->OnCreatedArchetype(old_size, *ptr);
				}
			}
		}

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
						return ref.current_count;
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

	void ComponentManager::FixComponentChunkHole_AssumedLocked(std::pmr::vector<RemovedColumn>& holes, void(*func)(ChunkView const& view, std::size_t, std::size_t))
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
					ite2.layout->CopyConstruction(target, source);
					ite2.layout->Destruction(source);
				}

				if(func != nullptr)
				{
					(*func)(mm, chunk.current_count, ite.column_index);
				}
			}
		}
		holes.clear();
	}
}