module;

#include <cassert>

module NoodlesQuery;

namespace Noodles
{

	ComponentQuery::Ptr ComponentQuery::Create(
		std::size_t archetype_container_count,
		std::size_t component_container_count,
		std::size_t require_count,
		void (*init_func)(void*, std::span<BitFlag> require, BitFlagContainerViewer writed, BitFlagContainerViewer refuse),
		void* append_data,
		std::pmr::memory_resource* resource
	)
	{
		auto layout = Potato::MemLayout::MemLayoutCPP::Get<ComponentQuery>();
		auto require_offset = layout.Insert(Potato::IR::Layout::GetArray<BitFlag>(require_count));
		auto bitflag_offset = layout.Insert(Potato::IR::Layout::GetArray<BitFlagContainerViewer::Element>(component_container_count * 3 + archetype_container_count));
		
		auto record = Potato::IR::MemoryResourceRecord::Allocate(resource, layout.Get());

		if (record)
		{

			auto require_span = std::span<BitFlag>{
				new (static_cast<void*>(record.GetByte(require_offset))) BitFlag[require_count],
				require_count
			};

			auto bitflag_conatiner_span = std::span<BitFlagContainerViewer::Element>{
				new (static_cast<void*>(record.GetByte(bitflag_offset))) std::size_t[component_container_count * 3 + archetype_container_count],
				component_container_count * 3 + archetype_container_count
			};

			BitFlagContainerViewer require = bitflag_conatiner_span.subspan(0, component_container_count);
			BitFlagContainerViewer refuse = bitflag_conatiner_span.subspan(component_container_count, component_container_count);
			BitFlagContainerViewer writed = bitflag_conatiner_span.subspan(component_container_count * 2, component_container_count);
			BitFlagContainerViewer archetype_usage = bitflag_conatiner_span.subspan(component_container_count * 3, archetype_container_count);
			require.Reset();
			refuse.Reset();
			writed.Reset();
			archetype_usage.Reset();

			(*init_func)(append_data, require_span, writed, refuse);

			for (auto ite : require_span)
			{
				auto re = require.SetValue(ite);
				assert(re);
			}

			assert(*require.Inclusion(writed));

			return new (record.Get()) ComponentQuery{ record, require, writed, refuse, archetype_usage, require_span };
		}

		return {};
	}

	bool ComponentQuery::UpdateQueryData(ComponentManager const& manager)
	{
		if (updated_archetype_count < manager.GetArchetypeCount())
		{
			bool has_been_modify = false;
			for (std::size_t index = updated_archetype_count; index < manager.GetArchetypeCount(); ++index)
			{
				if (manager.IsArchetypeAcceptQuery(index, require_bitflag_viewer, refuse_bitflag_viewer))
				{
					has_been_modify = true;
					++archetype_count;
					auto old_size = query_data.size();
					query_data.resize(query_data.size() + 1 + require_bitflag.size() * ComponentManager::GetQueryDataCount());
					query_data[old_size] = index;
					auto re = manager.TranslateClassToQueryData(index, require_bitflag, std::span(query_data).subspan(old_size + 1));
					assert(re);
					archetype_bitflag_viewer.SetValue(BitFlag{index});
				}
			}
			updated_archetype_count = manager.GetArchetypeCount();
			return has_been_modify;
		}
		return false;
	}

	std::optional<std::size_t> ComponentQuery::QueryComponentArrayWithIterator(ComponentManager& manager, std::size_t iterator, std::size_t chunk_index, std::span<void*> output_component)
	{
		if (iterator < archetype_count)
		{
			auto span = std::span<std::size_t>(query_data).subspan(query_data_fast_offset * iterator, query_data_fast_offset);
			auto re = manager.QueryComponentArray(span[0], chunk_index, span.subspan(1), output_component);
			if (re)
			{
				return re.Get();
			}
		}
		return {};
	}

	bool ComponentQuery::QueryComponent(ComponentManager& manager, ComponentManager::Index entity_index, std::span<void*> output_component)
	{
		auto re = archetype_bitflag_viewer.GetValue(BitFlag{entity_index.archetype_index});
		if (re.has_value() && *re)
		{
			auto span = std::span<std::size_t>(query_data);
			for (std::size_t i = 0; i < archetype_count; ++archetype_count)
			{
				auto ite_span = span.subspan(query_data_fast_offset * i, query_data_fast_offset);
				if (entity_index.archetype_index == ite_span[0])
				{
					return manager.QueryComponent(entity_index, ite_span.subspan(1), output_component);
				}
			}
		}
		return false;
	}

	SingletonQuery::Ptr SingletonQuery::Create(
		std::size_t singleton_container_count,
		std::size_t singleton_count,
		void (*init_func)(void*, std::span<BitFlag> require, BitFlagContainerViewer writed),
		void* append_data,
		std::pmr::memory_resource* resource
	)
	{
		auto layout = Potato::MemLayout::MemLayoutCPP::Get<ComponentQuery>();
		auto query_data_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<std::size_t>(singleton_count * SingletonManager::GetQueryDataCount()));
		auto require_offset = layout.Insert(Potato::IR::Layout::GetArray<BitFlag>(singleton_count));
		auto bitflag_offset = layout.Insert(Potato::IR::Layout::GetArray<BitFlagContainerViewer::Element>(singleton_container_count * 2));


		
		auto record = Potato::IR::MemoryResourceRecord::Allocate(resource, layout.Get());

		if (record)
		{
			auto query_data = std::span<std::size_t>{
				new (static_cast<void*>(record.GetByte(query_data_offset))) std::size_t[singleton_count * SingletonManager::GetQueryDataCount()],
				singleton_count* SingletonManager::GetQueryDataCount()
			};

			auto require_span = std::span<BitFlag>{
				new (static_cast<void*>(record.GetByte(require_offset))) BitFlag[singleton_count],
				singleton_count
			};

			auto bitflag_conatiner_span = std::span<BitFlagContainerViewer::Element>{
				new (static_cast<void*>(record.GetByte(bitflag_offset))) std::size_t[singleton_container_count * 2],
				singleton_container_count
			};

			BitFlagContainerViewer require = bitflag_conatiner_span.subspan(0, singleton_container_count);
			BitFlagContainerViewer writed = bitflag_conatiner_span.subspan(singleton_container_count, singleton_container_count);
			require.Reset();
			writed.Reset();

			(*init_func)(append_data, require_span, writed);

			for (auto ite : require_span)
			{
				auto re = require.SetValue(ite);
				assert(re);
			}

			assert(*require.Inclusion(writed));

			return new (record.Get()) SingletonQuery{ record, require, writed, require_span, query_data };
		}

		return {};
	}

	bool SingletonQuery::UpdateQueryData(SingletonManager const& manager)
	{
		if (current_version < manager.GetSingletonVersion())
		{
			current_version = manager.GetSingletonVersion();
			manager.TranslateBitFlagToQueryData(singleton_bitflag, query_data);
			return true;
		}
		return false;
	}

	bool SingletonQuery::QuerySingleton(SingletonManager const& manager, std::span<void*> output_component)
	{
		if (current_version == manager.GetSingletonVersion())
		{
			current_version = manager.GetSingletonVersion();
			manager.QuerySingletonData(query_data, output_component);
			return true;
		}
		return false;
	}

	/*
	ComponentQueryManager::ComponentQueryManager(GlobalContext::Ptr context, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

	void ComponentQueryManager::RegisterQueryIdentity(std::size_t query_identity)
	{

	}
	*/


	/*
	void ComponentQueryManager::RegisterQueryIndex(std::size_t query_index, std::span<>)
	{
		if (query_index < query_info.size())
		{
			query_info[query_index].reference_count += 1;
		}
		else {
			auto old_index = 
		}
	}
	*/





	/*
	

	std::optional<std::size_t> ComponentQuery::Update_AssumedLocked(StructLayoutManager& manager, std::size_t chunk_id, std::size_t chunk_size)
	{
		if(this->chunk_id != chunk_id)
		{
			if(this->manager != &manager)
			{
				return std::nullopt;
			}
			this->chunk_id = chunk_id;
			chunk_size_last_update = 0;
			MarkElement::Reset(archetype_usable);
			return 0;
		}else
		{
			if(chunk_size_last_update < chunk_size)
			{
				return chunk_size_last_update;
			}
		}
		return std::nullopt;
	}
	bool ComponentQuery::VersionCheck_AssumedLocked(std::size_t chunk_id, std::size_t chunk_size) const
	{
		if(this->chunk_id == chunk_id && this->chunk_size_last_update == chunk_size)
		{
			return true;
		}
		return false;
	}

	bool ComponentQuery::VersionCheck_AssumedLocked(std::size_t chunk_id) const
	{
		if (this->chunk_id == chunk_id)
		{
			return true;
		}
		return false;
	}

	bool ComponentQuery::OnCreatedArchetype_AssumedLocked(std::size_t archetype_index, Archetype const& archetype)
	{
		assert(archetype_index == chunk_size_last_update);
		chunk_size_last_update += 1;
		auto archetype_atomic_id = archetype.GetAtomicTypeMark();
		if (MarkElement::Inclusion(archetype_atomic_id, GetRequiredStructLayoutMarks().total_marks) && !MarkElement::IsOverlapping(archetype_atomic_id, refuse_component))
		{
			auto old_size = archetype_member.size();
			auto atomic_span = GetComponentMarkIndex();
			auto atomic_size = atomic_span.size();
			archetype_member.resize(old_size + 1 + atomic_size);
			auto span = std::span(archetype_member).subspan(old_size);
			span[0] = archetype_index;
			for (std::size_t i = 0; i < atomic_size; ++i)
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

	std::optional<std::span<std::size_t const>> ComponentQuery::EnumMountPointIndexByArchetypeIndex_AssumedLocked(std::size_t archetype_index) const
	{
		auto size = GetComponentMarkIndex().size();
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

	std::optional<std::span<std::size_t const>> ComponentQuery::EnumMountPointIndexByIterator_AssumedLocked(std::size_t ite_index, std::size_t& archetype_index) const
	{
		auto size = GetComponentMarkIndex().size();
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

	bool ComponentQuery::IsIsOverlappingRunTime(ComponentQuery const& other, std::span<MarkElement const> archetype_usage) const
	{
		std::shared_lock sl(mutex);
		std::shared_lock sl2(other.mutex);
		return
			MarkElement::IsOverlappingWithMask(archetype_usable, other.archetype_usable, archetype_usage) && (
				MarkElement::IsOverlapping(require_component, other.require_write_component)
				|| MarkElement::IsOverlapping(require_write_component, other.require_component)
				);
	}

	SingletonQuery::Ptr SingletonQuery::Create(
		StructLayoutManager& manager,
		std::span<StructLayoutWriteProperty const> require_singleton,
		std::pmr::memory_resource* storage_resource
	)
	{
		auto layout = Potato::MemLayout::MemLayoutCPP::Get<SingletonQuery>();
		auto require_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<MarkElement>(manager.GetSingletonStorageCount()));
		auto require_write_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<MarkElement>(manager.GetSingletonStorageCount()));
		auto atomic_index_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<MarkIndex>(require_singleton.size()));
		auto reference_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<std::size_t>(require_singleton.size()));
		auto re = Potato::IR::MemoryResourceRecord::Allocate(storage_resource, layout.Get());
		if (re)
		{
			std::span<MarkElement> require_mask{ new (re.GetByte(require_offset)) MarkElement[manager.GetSingletonStorageCount()], manager.GetSingletonStorageCount() };
			std::span<MarkElement> require_write_mask{ new (re.GetByte(require_write_offset)) MarkElement[manager.GetSingletonStorageCount()], manager.GetSingletonStorageCount() };
			std::span<MarkIndex> require_index{ new (re.GetByte(atomic_index_offset)) MarkIndex[require_singleton.size()], require_singleton.size() };
			std::span<std::size_t> reference_span{ new (re.GetByte(reference_offset)) std::size_t[require_singleton.size()], require_singleton.size() };

			for (auto& ite : reference_span)
			{
				ite = std::numeric_limits<std::size_t>::max();
			}

			auto ite_span = require_index;

			for (auto& Ite : require_singleton)
			{
				auto loc = manager.LocateSingleton(*Ite.struct_layout);
				if (loc.has_value())
				{
					MarkElement::Mark(require_mask, *loc);
					ite_span[0] = *loc;
					ite_span = ite_span.subspan(1);
					if (Ite.need_write)
					{
						MarkElement::Mark(require_write_mask, *loc);
					}
				}
				else
				{
					assert(false);
				}
			}

			return new (re.Get()) SingletonQuery{ re, require_mask, require_write_mask, require_index, reference_span, &manager };
		}
		return {};
	}

	bool SingletonQuery::OnSingletonModify_AssumedLocked(Archetype const& archetype)
	{
		auto span = GetMarkIndex();
		auto ref = archetype_offset;
		for (auto& ite : span)
		{
			auto loc = archetype.Locate(ite);
			if (loc.has_value())
			{
				ref[0] = archetype[loc->index].offset;
			}
			else
			{
				ref[0] = std::numeric_limits<std::size_t>::max();
			}
			ref = ref.subspan(1);
		}
		archetype_id = reinterpret_cast<std::size_t>(&archetype);
		return true;
	}

	bool SingletonQuery::Update_AssumedLocked(StructLayoutManager& manager, std::size_t archetype_id)
	{
		if (this->archetype_id != archetype_id)
		{
			if (this->manager != &manager)
			{
				return false;
			}
			this->archetype_id = 0;
			for (auto& ite : archetype_offset)
			{
				ite = std::numeric_limits<std::size_t>::max();
			}
			return true;
		}
		return false;
	}

	bool SingletonQuery::VersionCheck_AssumedLocked(std::size_t archetype_id) const
	{
		return this->archetype_id == archetype_id;
	}

	void SingletonQuery::Reset()
	{
		std::lock_guard lg(mutex);
		for (auto& ite : archetype_offset)
		{
			ite = std::numeric_limits<std::size_t>::max();
		}
	}

	ThreadOrderQuery::Ptr ThreadOrderQuery::Create(StructLayoutManager& manager, std::span<StructLayoutWriteProperty const> info, std::pmr::memory_resource* resource)
	{
		auto layout = Potato::MemLayout::MemLayoutCPP::Get<ThreadOrderQuery>();
		auto offset = layout.Insert(Potato::MemLayout::Layout::Get<MarkElement>(), manager.GetThreadOrderStorageCount() * 2);
		auto re = Potato::IR::MemoryResourceRecord::Allocate(resource, layout.Get());
		if (re)
		{
			std::span<MarkElement> write_span = {
				new(re.GetByte(offset)) MarkElement[manager.GetThreadOrderStorageCount()],
				manager.GetThreadOrderStorageCount()
			};

			std::span<MarkElement> total_span = {
				new(re.GetByte(offset) + sizeof(MarkElement) * manager.GetThreadOrderStorageCount()) MarkElement[manager.GetThreadOrderStorageCount()],
				manager.GetThreadOrderStorageCount()
			};

			for (auto& ite : info)
			{
				auto loc = manager.LocateThreadOrder(*ite.struct_layout);
				if (!loc)
				{
					re.Deallocate();
					return {};
				}
				else
				{
					if (ite.need_write)
					{
						MarkElement::Mark(write_span, *loc);
					}
					MarkElement::Mark(total_span, *loc);
				}
			}

			return new (re.Get()) ThreadOrderQuery{ re, {write_span, total_span} };
		}
		return {};
	}
	*/
}