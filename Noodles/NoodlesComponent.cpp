module;

#include <cassert>

module NoodlesComponent;

constexpr std::size_t component_page_min_element_count = 10;
constexpr std::size_t component_page_min_size = 4096;

namespace Noodles
{

	auto ComponentPage::Create(
		Potato::IR::Layout component_layout, std::size_t min_element_count, std::size_t min_page_size, std::pmr::memory_resource* up_stream
	)-> Ptr
	{
		if(up_stream != nullptr)
		{
			auto self_size = sizeof(ComponentPage);
			auto fix_size = 0;
			if (component_layout.Align > alignof(ComponentPage))
			{
				fix_size = component_layout.Align - alignof(ComponentPage);
			}
			std::size_t element_count = 0;
			while (true)
			{
				auto buffer_size = (min_page_size - fix_size - self_size);
				element_count = buffer_size / component_layout.Size;
				if (element_count >= min_element_count)
				{
					break;
				}
				else
				{
					min_page_size *= 2;
				}
			}
			auto adress = up_stream->allocate(min_page_size, alignof(ComponentPage));
			if(adress != nullptr)
			{
				void* buffer = reinterpret_cast<ComponentPage*>(adress) + 1;
				std::size_t require_size = component_layout.Align * 2;
				if(std::align(component_layout.Align, component_layout.Align, buffer, require_size) != nullptr)
				{
					auto start = static_cast<std::byte*>(buffer);
					assert(static_cast<std::byte*>(adress) + min_page_size >= start + component_layout.Size * min_element_count);
					std::span<std::byte> real_buffer{
						start,
						start + component_layout.Size * min_element_count
					};
					Ptr ptr = new (adress) ComponentPage{
						element_count,
						min_page_size,
						up_stream,
						real_buffer
					};
					return ptr;
				}else
				{
					up_stream->deallocate(adress, min_page_size, alignof(ComponentPage));
				}
			}

		}
		return {};

	}

	ComponentPage::ComponentPage(
		std::size_t max_element_count,
		std::size_t allocate_size,
		std::pmr::memory_resource* upstream,
		std::span<std::byte> buffer
	) : max_element_count(max_element_count),
		buffer(buffer),
		allocate_size(allocate_size),
		resource(upstream)
	{
		
	}

	void ComponentPage::Release()
	{
		auto o_resource = resource;
		auto size = allocate_size;
		assert(o_resource != nullptr);
		this->~ComponentPage();
		o_resource->deallocate(this, size, alignof(ComponentPage));
	}

	bool EntityConstructor::Construct(UniqueTypeID const& id, void* source, std::size_t i)
	{
		assert(*this);
		auto index = archetype_ptr->LocateTypeID(id, i);
		if(index.has_value())
		{
			if(construct_record[*index] == 0)
			{
				auto tar_buffer = archetype_ptr->GetData(*index, mount_point);
				assert(tar_buffer != nullptr);
				archetype_ptr->MoveConstruct(*index, tar_buffer, source);
				construct_record[*index] = 1;
				return true;
			}
		}
		return false;
	}

	/*
	auto ComponentFilterWrapper::Create(std::span<UniqueTypeID const> ids, std::pmr::memory_resource* resource)
		-> Ptr
	{
		static_assert(alignof(ComponentFilterWrapper) == alignof(UniqueTypeID));
		if(resource != nullptr)
		{
			std::size_t total_size = sizeof(ComponentFilterWrapper) + sizeof(UniqueTypeID) * ids.size();
			auto adress = resource->allocate(
				total_size, alignof(ComponentFilterWrapper)
			);
			if(adress != nullptr)
			{
				Ptr ptr = new (adress) ComponentFilterWrapper{
					std::span<std::byte>{
						static_cast<std::byte*>(adress) + sizeof(ComponentFilterWrapper),
						total_size - sizeof(ComponentFilterWrapper)
					},
					ids,
					total_size,
					resource
				};
				return ptr;
			}
		}
		return {};
	}

	ComponentFilterWrapper::ComponentFilterWrapper(
		std::span<std::byte> buffer,
		std::span<UniqueTypeID const> ref_ids,
		std::size_t allocated_size, std::pmr::memory_resource* up_stream
	) : allocated_size(allocated_size), resource(up_stream),
		in_direct_mapping(up_stream), archetype_id_index(up_stream)
	{
		assert(buffer.size() <= ref_ids.size() * sizeof(UniqueTypeID));

		std::span<UniqueTypeID> ite_span {
			reinterpret_cast<UniqueTypeID*>(buffer.data()),
			ref_ids.size()
		};

		std::size_t i = 0;

		for(auto& ite : ref_ids)
		{

			auto ai = std::find(
				ite_span.begin(),
				ite_span.begin() + i,
				ite
			);
			if(ai == ite_span.begin() + i)
			{
				assert(i < ite_span.size());
				new (&ite_span[i]) UniqueTypeID{ ite };
				++i;
			}
		}
		capture_info = ite_span.subspan(0, i);

		std::sort(
			capture_info.begin(),
			capture_info.end(),
			[](UniqueTypeID& i1, UniqueTypeID& i2){ return i1<=> i2 == std::strong_ordering::less;  }
			);

		assert(!capture_info.empty());
	}

	void ComponentFilterWrapper::Release()
	{
		auto old_resource = resource;
		assert(old_resource != nullptr);
		auto az = allocated_size;
		this->~ComponentFilterWrapper();
		old_resource->deallocate(
			this, az, alignof(ComponentFilterWrapper)
		);
	}

	ComponentFilterWrapper::~ComponentFilterWrapper()
	{
		for(auto& ite : capture_info)
		{
			ite.~UniqueTypeID();
		}
		capture_info = {};
	}

	bool ComponentFilterWrapper::IsSame(std::span<UniqueTypeID const> ids) const
	{
		if(ids.size() == capture_info.size())
		{
			for(std::size_t i =0; i< capture_info.size(); ++i)
			{
				if(capture_info[i] != ids[i])
					return false;
			}
			return true;
		}
		return false;
	}

	bool ComponentFilterWrapper::TryInsertCollection(std::size_t element_index, Archetype const& archetype)
	{
		std::lock_guard lg(filter_mutex);
		std::size_t old_size = archetype_id_index.size();
		std::size_t max_size = archetype.GetTypeIDCount();
		for (auto& ite : capture_info)
		{
			auto locate_index = archetype.LocateTypeID(ite);
			if (locate_index.has_value())
			{
				std::size_t count = 1;
				std::size_t next = *locate_index + 1;
				while (next < max_size)
				{
					auto re = archetype.GetTypeID(next);
					if (re == ite)
					{
						++count;
						++next;
					}
					else
						break;
				}
				archetype_id_index.push_back({ *locate_index, count });
			}
			else
			{
				archetype_id_index.resize(old_size);
				return false;
			}
		}
		in_direct_mapping.emplace_back(
			element_index,
			Potato::Misc::IndexSpan<>{old_size, archetype_id_index.size()}
		);
		return true;
	}

	std::size_t ComponentFilterWrapper::UniqueAndSort(std::span<UniqueTypeID> ids)
	{
		std::sort(ids.begin(), ids.end());
		auto last = std::unique(ids.begin(), ids.end());
		return std::distance(ids.begin(), last);
	}

	std::optional<std::size_t> ComponentFilterWrapper::LocateTypeIDIndex(UniqueTypeID const& id) const
	{
		auto find = std::find(capture_info.begin(), capture_info.end(), id);
		if(find != capture_info.end())
		{
			return std::distance(capture_info.begin(), find);
		}
		return std::nullopt;
	}

	ComponentFilterWrapper::Ptr ArchetypeComponentManager::CreateFilter(std::span<UniqueTypeID const> ids, std::pmr::memory_resource* resource)
	{
		std::lock_guard lg(filter_mapping_mutex);

		std::size_t total_count = 0;
		for(auto& ite : filter_mapping)
		{
			assert(ite);
			if(ite->IsSame(ids))
			{
				return ComponentFilterWrapper::Ptr{ite.GetPointer()};
			}
		}

		auto ptr = ComponentFilterWrapper::Create(ids, resource);
		filter_mapping.push_back(ptr.GetPointer());
		std::shared_lock lg2(components_mutex);
		std::size_t i = 0;
		for(auto& ite : components)
		{
			ptr->TryInsertCollection(i, *ite.archetype);
			++i;
		}
		return ptr;
	}
	*/

	
	
	ArchetypeComponentManager::ArchetypeComponentManager(std::pmr::memory_resource* upstream)
		:components(upstream), spawned_entities(upstream), spawned_entities_resource(upstream),
		archetype_resource(upstream), entity_resource(decltype(entity_resource)::Type::Create(upstream)),
		removed_entities(upstream), components_resource(upstream)
	{
		
	}

	ArchetypeComponentManager::~ArchetypeComponentManager()
	{
		{
			std::lock_guard lg(spawn_mutex);
			for (auto& ite : spawned_entities)
			{
				assert(ite);
				ReleaseEntity(*ite);
				ite->status = EntityStatus::Destroy;
			}

			removed_entities.clear();
			spawned_entities_resource.release();
		}

		{
			UniqueTypeID entity_id = UniqueTypeID::Create<EntityProperty>();
			std::lock_guard lg(components_mutex);

			for (auto& ite : components)
			{
				assert(ite.archetype);
				auto top = std::move(ite.top_page);
				auto index = ite.archetype->LocateTypeID(entity_id);
				assert(index.has_value());
				while (top)
				{
					for (auto ite2 : *top)
					{
						auto entity = static_cast<EntityProperty*>(ite.archetype->GetData(*index, ite2))->entity;
						assert(entity);
						std::lock_guard lg(entity->mutex);
						ReleaseEntity(*entity);
						entity->status = EntityStatus::Destroy;
					}
					top = std::move(top->next_page);
				}
				ite.archetype = {};
				ite.last_page = {};
				ite.top_page = {};
			}
			components.clear();
		}

		{
			std::lock_guard lg(filter_mapping_mutex);
			filter_mapping.clear();
		}
		need_update = false;
	}

	auto ArchetypeComponentManager::PreCreateEntityImp(std::span<ArchetypeID const> ids, std::pmr::memory_resource* resource)
		->EntityConstructor
	{
		static std::array<ArchetypeID, 1> build_in = {
			ArchetypeID::Create<EntityProperty>()
		};

		std::lock_guard lg(spawn_mutex);
		Archetype::Ptr ptr = Archetype::Create(ids, std::span(build_in), &spawned_entities_resource);
		if (ptr)
		{
			auto layout = ptr->GetLayout();
			ArchetypeMountPoint mp{
			spawned_entities_resource.allocate(layout.Size, layout.Align)
			};
			if (mp)
			{
				return EntityConstructor{
					EntityConstructor::Status::Done,
					std::move(ptr),
					mp,
					resource
				};
			}else
			{
				return EntityConstructor{
					EntityConstructor::Status::BadMemoryResource
				};
			}
		}else
		{
			return EntityConstructor{
				EntityConstructor::Status::BadArchetype,
			};
		}
	}

	Entity ArchetypeComponentManager::CreateEntityImp(EntityConstructor& constructor)
	{

		if(constructor.archetype_ptr && constructor.mount_point)
		{
			auto loc = constructor.archetype_ptr->LocateTypeID(
				UniqueTypeID::Create<EntityProperty>()
			);

			
			if(loc.has_value() && constructor.construct_record[*loc] == 0)
			{
				std::size_t cur_i = 0;
				for(auto ite : constructor.construct_record)
				{
					if(ite == 0 && cur_i != *loc)
					{
						constructor.archetype_ptr->DefaultConstruct(cur_i, 
							constructor.archetype_ptr->GetData(cur_i, constructor.mount_point)
						);
					}
					++cur_i;
				}
				assert(entity_resource);
				Entity entity = EntityStorage::Create(entity_resource->get_resource_interface());
				if(entity)
				{
					{
						std::shared_lock sl(components_mutex);
						std::size_t index = 0;
						for(auto& ite : components)
						{
							assert(ite.archetype);
							if(ite.archetype->operator<=>(*constructor.archetype_ptr) == std::strong_ordering::equivalent)
							{
								constructor.archetype_ptr->fast_index = index + 1;
							}
							++index;
						}
					}
					entity->archetype = std::move(constructor.archetype_ptr);
					entity->mount_point = constructor.mount_point;
					auto buffer = entity->archetype->GetData(*loc, entity->mount_point);
					EntityProperty pro{
						entity
					};
					entity->archetype->MoveConstruct(*loc, buffer, &pro);
					std::lock_guard lg(spawn_mutex);
					spawned_entities.push_back(entity);
					need_update = true;
					return entity;
				}
			}
		}
		return {};
	}

	void ArchetypeComponentManager::ReleaseEntity(EntityStorage& storage)
	{
		assert(storage.archetype && storage.mount_point);
		for (std::size_t i = 0; i < storage.archetype->GetTypeIDCount(); ++i)
		{
			auto buffer = storage.archetype->GetData(i, storage.mount_point);
			assert(buffer != nullptr);
			storage.archetype->Destruction(i, buffer);
		}
		storage.mount_point = {};
		storage.archetype = {};
	}

	bool ArchetypeComponentManager::DestroyEntity(Entity entity)
	{
		if(entity)
		{
			std::lock_guard lg(entity->mutex);
			if(entity->resource == entity_resource->get_resource_interface())
			{
				if(
					entity->status != EntityStatus::PendingDestroy
					|| entity->status != EntityStatus::Destroy
					)
				{
					auto last_status = entity->status;
					std::lock_guard lg(spawn_mutex);
					entity->status = EntityStatus::PendingDestroy;
					removed_entities.push_back({
						std::move(entity), last_status
					});
				}
				
				return true;
			}
		}
		return false;
	}

	bool ArchetypeComponentManager::UpdateEntityStatus()
	{
		std::lock_guard lg(spawn_mutex);
		if(need_update)
		{
			need_update = false;
			std::lock_guard lg2(components_mutex);
			while(!removed_entities.empty())
			{
				auto [top, status] = std::move(*removed_entities.rbegin());
				removed_entities.pop_back();
				assert(top);
				std::lock_guard lg3(top->mutex);
				if(top->archetype)
				{
					assert(top->status == EntityStatus::PendingDestroy);
					if(status == EntityStatus::PreInit)
					{
						auto find = std::find(
							spawned_entities.begin(),
							spawned_entities.end(),
							top
						);
						assert(find != spawned_entities.end());
						top->status = EntityStatus::Destroy;
						ReleaseEntity(*top);
						std::swap(*find, *spawned_entities.rbegin());
						spawned_entities.pop_back();
						continue;
					}else
					{
						auto fi = top->archetype->GetFastIndex();
						assert(fi != 0 && fi <= components.size());
						auto art = components[fi - 1].archetype;
						assert(art);
						assert(*art <=> *top->archetype == std::strong_ordering::equivalent);
						auto old_mp = top->mount_point;
						ReleaseEntity(*top);
						top->status = EntityStatus::Destroy;
						
						auto find = std::find_if(
							spawned_entities.begin(),
							spawned_entities.end(),
							[=](Entity const& E)
							{
								assert(E && E->archetype);
								auto Tfi = E->archetype->GetFastIndex();
								return fi == Tfi;
							}
						);

						if(find == spawned_entities.end())
						{
							auto& ref = components[fi - 1];
							assert(ref.last_page);
							auto mp = ref.last_page->end();
							mp -= 1;
							if(old_mp != mp)
							{
								for (std::size_t i = 0; i < art->GetTypeIDCount(); ++i)
								{
									auto tar_buffer = art->GetData(i, mp);
									art->MoveConstruct(
										i, art->GetData(i, old_mp),
										tar_buffer
									);
									art->Destruction(
										i,
										tar_buffer
									);
								}
							}

							ref.last_page->available_count -= 1;
							if(ref.last_page->available_count == 0)
							{
								assert(ref.top_page);
								if(ref.top_page == ref.last_page)
								{
									ref.top_page.Reset();
									ref.last_page.Reset();
								}else
								{
									auto last_page = ref.top_page;
									while(true)
									{
										if(!last_page->next_page)
										{
											assert(last_page->available_count == 0);
											ref.last_page = std::move(last_page);
											break;
										}else
										{
											last_page = last_page->next_page;
										}
									}
								}
							}
						}else
						{
							auto pre_init = std::move(*find);
							std::swap(*find, *spawned_entities.rbegin());
							spawned_entities.pop_back();
							assert(*art <=> *pre_init->archetype == std::strong_ordering::equivalent);
							for(std::size_t i = 0; i < art->GetTypeIDCount(); ++i)
							{
								art->MoveConstruct(
									i, art->GetData(i, old_mp),
									art->GetData(i, pre_init->mount_point)
								);
							}
							ReleaseEntity(*pre_init);
							pre_init->archetype = std::move(art);
							pre_init->mount_point = old_mp;
							pre_init->status = EntityStatus::Normal;
						}
					}
				}
			}

			std::size_t cache_size = components.size();
			while(!spawned_entities.empty())
			{
				auto top = std::move(*spawned_entities.rbegin());
				spawned_entities.pop_back();
				assert(top->archetype && top->mount_point);
				auto fi = top->archetype->GetFastIndex();
				assert(fi <= cache_size);
				if(fi == 0)
				{
					auto cur_span = std::span(components).subspan(cache_size);
					std::size_t i = 0;
					for(auto& ite : cur_span)
					{
						if(*ite.archetype <=> *top->archetype == std::strong_ordering::equivalent)
						{
							fi = i + cache_size + 1;
							break;
						}
						++i;
					}
					if(fi == 0)
					{
						std::lock_guard lg(archetype_resource_mutex);
						auto new_archetype = top->archetype->Clone(&archetype_resource);
						new_archetype->fast_index = components.size();
						components.push_back({
							std::move(new_archetype),
							{},
							{}
						});
						fi = components.size();
					}
				}
				assert(fi != 0);
				auto& ref = components[fi - 1];
				assert(ref.archetype);
				auto art = ref.archetype;
				if(ref.last_page)
				{
					auto last = ref.last_page->GetLastMountPoint();
					auto max = ref.last_page->GetMaxMountPoint();
					if(max == last)
					{
						auto new_page = ComponentPage::Create(
							art->GetBufferLayout(),
							component_page_min_element_count,
							component_page_min_size,
							&components_resource
						);
						ref.last_page->next_page = new_page;
						ref.last_page = std::move(new_page);
					}
				}else
				{
					assert(!ref.top_page);
					auto new_page = ComponentPage::Create(
						art->GetBufferLayout(),
						component_page_min_element_count,
						component_page_min_size,
						&components_resource
					);
					ref.top_page = new_page;
					ref.last_page = std::move(new_page);
				}
				assert(ref.last_page);

				auto mp = ref.last_page->GetLastMountPoint();
				assert(ref.last_page->GetMaxMountPoint() != mp);

				std::size_t archetype_id_count = art->GetTypeIDCount();
				for (std::size_t i = 0; i < archetype_id_count; ++i)
				{
					auto buf = art->GetData(i, top->mount_point);
					art->MoveConstruct(
						i,
						art->GetData(i, mp),
						buf
					);
					art->Destruction(
						i,
						buf
					);
				}

				ref.last_page->available_count += 1;

				top->archetype = std::move(art);
				top->mount_point = mp;
				top->status = EntityStatus::Normal;
			}

			if(cache_size < components.size())
			{
				std::lock_guard lg3(filter_mapping_mutex);
				for(auto& ite : filter_mapping)
				{
					for (std::size_t i = cache_size; i < components.size(); ++i)
					{
						assert(components[i].archetype);
						ite.filter->TryPreCollection(
							i,
							*components[i].archetype
						);
					}
				}
			}
			return true;
		}
		return false;
	}

	bool ArchetypeComponentManager::RegisterComponentFilter(ComponentFilterInterface::Ptr ptr, std::size_t group_id)
	{
		if(ptr)
		{
			{
				std::shared_lock sl(components_mutex);
				std::size_t i = 0;
				for (auto& ite : components)
				{
					ptr->TryPreCollection(i, *ite.archetype);
					++i;
				}
			}
			{
				std::lock_guard lg(filter_mapping_mutex);
				filter_mapping.emplace_back(
					std::move(ptr),
					group_id
				);
			}
			
			return true;
		}
		return false;
	}

	std::size_t ArchetypeComponentManager::ErasesComponentFilter(std::size_t group_id)
	{
		std::lock_guard lg(filter_mapping_mutex);
		std::size_t old_size = filter_mapping.size();
		std::erase_if(
			filter_mapping,
			[=](CompFilterElement const& ptr)
			{
				return ptr.group_id == group_id;
			}
		);
		return old_size - filter_mapping.size();
	}


	/*
	std::size_t ArchetypeComponentManager::ForeachMountPoint(ComponentFilterWrapper::Block block, void(*func)(void*, MountPointRange), void* data)
	{
		std::shared_lock lg(components_mutex);
		if (block.element_index < components.size())
		{
			std::size_t page_count = 0;
			auto& ref = components[block.element_index];
			auto top = ref.top_page;
			while (top)
			{
				MountPointRange mpr{ *ref.archetype, top->begin(), top->end() };
				func(data, mpr);
				++page_count;
				top = top->GetNextPage();
			}
			return page_count;
		}
		else
		{
			return 0;
		}
	}

	bool ArchetypeComponentManager::ReadEntity(EntityStorage const& entity, void(*func)(void*, Archetype const&, ArchetypeMountPoint), void* data)
	{
		if(entity.resource == entity_resource->get_resource_interface())
		{
			std::shared_lock lg(components_mutex);
			if(entity.status != EntityStatus::Destroy)
			{
				func(data, *entity.archetype, entity.mount_point);
				return true;
			}
		}
		return false;
	}
	*/
}