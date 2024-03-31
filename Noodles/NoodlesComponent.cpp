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
						reinterpret_cast<std::byte*>(adress) + min_page_size
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

	void Entity::SetFree()
	{
		status = EntityStatus::Free;
		archetype.Reset();
		mount_point = {};
		owner_id = 0;
		archetype_index = std::numeric_limits<std::size_t>::max();
	}

	auto Entity::Create(std::pmr::memory_resource* resource)
		-> Ptr
	{
		auto record = Potato::IR::MemoryResourceRecord::Allocate(resource, Potato::IR::Layout::Get<Entity>());
		if (record)
		{
			Ptr TPtr{ new (record.Get()) Entity{record } };
			return TPtr;
		}
		
		return {};
	}

	void Entity::Release()
	{
		auto orecord = record;
		this->~Entity();
		orecord.Deallocate();
	}

	Entity::Entity(Potato::IR::MemoryResourceRecord record)
		: record(record)
	{
		
	}

	bool FilterInterface::Register(std::size_t in_owner_id)
	{
		std::lock_guard lg(filter_mutex);
		if(owner_id == 0 && in_owner_id != 0)
		{
			owner_id = in_owner_id;
			return true;
		}
		return false;
	}

	bool FilterInterface::Unregister(std::size_t in_owner_id)
	{
		std::lock_guard lg(filter_mutex);
		if (owner_id != 0 && owner_id == in_owner_id)
		{
			owner_id = 0;
			return true;
		}
		return false;
	}

	void ComponentFilterInterface::OnCreatedArchetype(std::size_t in_owner_id, std::size_t archetype_index, Archetype const& archetype)
	{
		std::lock_guard lg(filter_mutex);
		if(owner_id == in_owner_id)
		{
			auto aspan = GetArchetypeIndex();
			assert(aspan.size() > 0);
			auto old_size = indexs.size();
			indexs.resize(old_size + aspan.size() + 1);
			auto out_span = std::span(indexs).subspan(old_size);
			out_span[0] = archetype_index;
			out_span = out_span.subspan(1);
			for (auto& ite : aspan)
			{
				auto ind = archetype.LocateTypeID(ite);
				if (ind.has_value())
				{
					out_span[0] = *ind;
					out_span = out_span.subspan(1);
				}
				else
				{
					indexs.resize(old_size);
					return;
				}
			}
		}
	}

	std::optional<std::size_t> ComponentFilterInterface::EnumByArchetypeIndex(std::size_t in_owner_id, std::size_t archetype_index, std::span<std::size_t> output_index) const
	{
		std::lock_guard lg(filter_mutex);
		if(owner_id == in_owner_id)
		{
			auto size = GetArchetypeIndex().size();
			auto span = std::span(indexs);
			assert((span.size() % (size + 1)) == 0);
			assert(output_index.size() >= size);
			while(!span.empty())
			{
				if(span[0] == archetype_index)
				{
					std::memcpy(output_index.data(), span.data() + 1, sizeof(std::size_t) * size);
					return size;
				}else
				{
					span = span.subspan(size + 1);
				}
			}
		}
		return std::nullopt;
	}

	std::optional<std::size_t> ComponentFilterInterface::EnumByIteratorIndex(std::size_t in_owner_id, std::size_t ite_index, std::size_t& archetype_index, std::span<std::size_t> output_index) const
	{
		std::lock_guard lg(filter_mutex);
		if (owner_id == in_owner_id)
		{
			auto size = GetArchetypeIndex().size();
			auto span = std::span(indexs);
			assert((span.size() % (size + 1)) == 0);
			assert(output_index.size() >= size);
			auto offset = ite_index * (size + 1);
			if(offset < span.size())
			{
				span = span.subspan(offset);
				archetype_index = span[0];
				std::memcpy(output_index.data(), span.data() + 1, sizeof(std::size_t) * size);
				return size;
			}
		}
		return std::nullopt;
	}

	std::tuple<Archetype::Ptr, ArchetypeMountPoint, std::size_t> ArchetypeComponentManager::ReadEntity(Entity const& entity, ComponentFilterInterface const& interface, std::span<std::size_t> output_index) const
	{
		Archetype::Ptr arc;
		std::size_t archetype_index;
		ArchetypeMountPoint mp;
		{
			std::shared_lock sl(entity.mutex);
			if (entity.owner_id != reinterpret_cast<std::size_t>(this))
			{
				return { {}, {}, 0 };
			}
			arc = entity.archetype;
			archetype_index = entity.archetype_index;
			mp = entity.mount_point;
		}
		if(arc)
		{
			auto re = interface.EnumByArchetypeIndex(reinterpret_cast<std::size_t>(this), archetype_index, output_index);
			if(re)
			{
				return {arc, mp, *re};
			}
		}
		return {{}, {}, 0};
	}


	ArchetypeComponentManager::ArchetypeComponentManager(std::pmr::memory_resource* upstream)
		:components(upstream), spawned_entities(upstream), temp_resource(upstream),
		archetype_resource(upstream),components_resource(upstream)
	{
		
	}

	
	ArchetypeComponentManager::~ArchetypeComponentManager()
	{
		{
			std::lock_guard lg(filter_mapping_mutex);
			for(auto& ite : filter_mapping)
			{
				auto& ref = *ite.filter;
				std::lock_guard lg(ref.filter_mutex);
				ref.Unregister(reinterpret_cast<size_t>(this));
				ref.Reset();
			}
			filter_mapping.clear();
			for(auto& ite : singleton_filters)
			{
				auto& ref = *ite.ptr;
				std::lock_guard lg(ref.filter_mutex);
				ref.Unregister(reinterpret_cast<size_t>(this));
				ref.Reset();
			}
			singleton_filters.clear();
		}

		{
			std::lock_guard lg2(singletons_mutex);
			singletons.clear();
		}

		{
			std::lock_guard lg(spawn_mutex);
			for(auto& ite : spawned_entities)
			{
				if(ite.status == SpawnedStatus::New || ite.status == SpawnedStatus::NewButNeedRemove)
				{
					auto& ref = *ite.entity;
					std::lock_guard lg3(ref.mutex);

					ref.archetype->Destruction(ref.mount_point);
					auto layout = ref.archetype->GetSingleLayout();
					temp_resource.deallocate(ref.mount_point.GetBuffer(0), layout.Size, layout.Size);
					ref.SetFree();
				}
			}
		}

		{
			std::lock_guard lg2(component_mutex);
			for(auto& ite : components)
			{
				auto ite2 = ite.top_page;
				while(ite2)
				{
					for(auto ite3 : *ite2)
					{
						ite.archetype->Destruction(ite3);
					}
					auto temp = std::move(ite2->GetNextPage());
					ite2 = std::move(temp);
				}
			}
			components.clear();
		}
		
	}

	bool ArchetypeComponentManager::CheckIsSameArchetype(Archetype const& target, std::size_t hash_code, std::span<ArchetypeID const> ids, std::span<std::size_t> output)
	{
		if(target.GetHashCode() == hash_code && target.GetElementCount() == ids.size())
		{
			auto ite_span = output;
			bool error = false;
			for (auto& ite2 : ids)
			{
				auto loc = target.LocateTypeID(ite2.id);
				if (loc.has_value())
				{
					ite_span[0] = *loc;
					ite_span = ite_span.subspan(1);
				}
				else
				{
					return false;
				}
			}
			return true;
		}
		return false;
	}

	std::tuple<Archetype::Ptr, ArchetypeMountPoint, std::size_t> ArchetypeComponentManager::CreateArchetype(std::span<ArchetypeID const> ids, std::span<std::size_t> output)
	{
		assert(ids.size() >= output.size());
		assert(Archetype::CheckUniqueArchetypeID(ids));
		std::size_t hash_code = 0;
		for(auto& ite : ids)
		{
			hash_code += ite.id.HashCode();
		}

		Archetype::Ptr archetype_ptr;
		std::size_t archetype_index = 0;

		{
			std::lock_guard sl2(component_mutex);

			for(auto& ite : components)
			{
				if(CheckIsSameArchetype(*ite.archetype, hash_code, ids, output))
				{
					archetype_ptr = ite.archetype;
					break;
				}else
				{
					archetype_index += 1;
				}
			}

			if(!archetype_ptr)
			{
				std::pmr::vector<ArchetypeID> temp_ids(&temp_resource);
				temp_ids.reserve(ids.size());
				for (auto& ite : ids)
				{
					temp_ids.push_back(ite);
				}
				std::sort(temp_ids.begin(), temp_ids.end(), [](ArchetypeID const& a1, ArchetypeID const& a2)
					{
						return a1 > a2;
					});
				archetype_ptr = Archetype::Create(temp_ids, &archetype_resource);

				if (archetype_ptr)
				{
					components.emplace_back(
						archetype_ptr,
						ComponentPage::Ptr{},
						ComponentPage::Ptr{},
						0
					);
					{
						std::shared_lock sl(filter_mapping_mutex);
						for (auto& ite : filter_mapping)
						{
							if (ite.filter)
							{
								ite.filter->OnCreatedArchetype(reinterpret_cast<std::size_t>(this), archetype_index, *archetype_ptr);
							}
						}
					}
					auto ite_span = output;
					for (auto ite : ids)
					{
						auto re = archetype_ptr->LocateTypeID(ite.id);
						assert(re);
						ite_span[0] = *re;
						ite_span = ite_span.subspan(1);
					}
				}
			}
		}


		if (archetype_ptr)
		{
			auto mp = Potato::IR::MemoryResourceRecord::Allocate(&temp_resource, archetype_ptr->GetSingleLayout());
			return { archetype_ptr, {mp.Get(), 1, 0}, archetype_index };
		}

		return { {}, ArchetypeMountPoint{}, 0 };
	}

	bool ArchetypeComponentManager::RegisterFilter(ComponentFilterInterface::Ptr ptr, std::size_t group_id)
	{
		if(ptr && ptr->Register(reinterpret_cast<std::size_t>(this)))
		{
			std::size_t  index = 0;
			{
				std::lock_guard lg(component_mutex);
				for(auto& ite : components)
				{
					assert(ite.archetype);
					ptr->OnCreatedArchetype(reinterpret_cast<std::size_t>(this), index, *ite.archetype);
					++index;
				}
			}
			std::lock_guard lg(filter_mapping_mutex);
			filter_mapping.emplace_back(std::move(ptr), group_id);
			return true;
		}
		return false;
	}

	bool ArchetypeComponentManager::RegisterFilter(SingletonFilterInterface::Ptr ptr, std::size_t group_id)
	{
		if(ptr && ptr->Register(reinterpret_cast<std::size_t>(this)))
		{
			std::lock_guard lg(ptr->filter_mutex);
			if(ptr->owner_id == 0)
			{
				ptr->owner_id = reinterpret_cast<std::size_t>(this);
				ptr->singleton_reference.Reset();
				auto RID = ptr->RequireTypeID();

				{
					std::lock_guard lg(singletons_mutex);
					for (auto& ite : singletons)
					{
						if (ite.id == RID)
						{
							ptr->singleton_reference = ite.single;
							break;
						}
					}
				}
				singleton_filters.emplace_back(std::move(ptr), group_id);
				return true;
			}
		}
		return false;
	}

	bool ArchetypeComponentManager::ReleaseEntity(Entity::Ptr ptr)
	{
		if(ptr)
		{
			std::lock_guard lg(ptr->mutex);
			if (ptr->owner_id != reinterpret_cast<std::size_t>(this))
				return false;
			switch(ptr->status)
			{
			case EntityStatus::PreInit:
				{
					std::lock_guard lg(spawn_mutex);
					for(auto& ite : spawned_entities)
					{
						if(ite.entity == ptr)
						{
							assert(ite.status == SpawnedStatus::New);
							ite.status = SpawnedStatus::NewButNeedRemove;
							ptr->status = EntityStatus::PendingDestroy;
							return true;
						}
					}
				}
				break;
			case EntityStatus::Normal:
			{
				std::lock_guard lg(spawn_mutex);
				spawned_entities.emplace_back(
					ptr, SpawnedStatus::RemoveOld
				);
				ptr->status = EntityStatus::PendingDestroy;
				return true;
			}
			default:
				break;
			}
		}
		return false;
	}

	bool ArchetypeComponentManager::ForceUpdateState()
	{
		bool Updated = false;

		{
			std::lock_guard lg(component_mutex);

			if (need_update)
			{
				need_update = false;
				Updated = true;

				struct EmptyEntity
				{
					std::size_t archetype_index;
					ArchetypeMountPoint mp;
					bool used = false;
				};

				std::pmr::vector<EmptyEntity> removed_entity(&temp_resource);

				std::lock_guard lg3(spawn_mutex);

				for (auto& ite : spawned_entities)
				{
					if (ite.status != SpawnedStatus::New)
					{
						std::lock_guard lg(ite.entity->mutex);
						ite.entity->archetype->Destruction(
							ite.entity->mount_point
						);

						if (ite.status == SpawnedStatus::RemoveOld)
						{
							removed_entity.emplace_back(
								ite.entity->archetype_index, ite.entity->mount_point, false);
						}
						ite.entity->SetFree();
					}
				}

				for (auto& ite : spawned_entities)
				{
					if (ite.status == SpawnedStatus::New)
					{
						std::lock_guard lg(ite.entity->mutex);
						auto mp = ite.entity->mount_point;
						auto index = ite.entity->archetype_index;
						auto& archetype = *ite.entity->archetype;
						bool Exist = false;
						for (auto& ite2 : removed_entity)
						{
							if (!ite2.used && ite2.archetype_index == index)
							{
								ite2.used = true;
								archetype.MoveConstruct(ite2.mp, mp);
								archetype.Destruction(mp);
								auto layout = archetype.GetSingleLayout();
								temp_resource.deallocate(mp.GetBuffer(0), layout.Size, layout.Align);
								ite.entity->mount_point = ite2.mp;
								ite.entity->status = EntityStatus::Normal;
								Exist = true;
								break;
							}
						}
						if (!Exist)
						{
							auto& ref = components[index];
							auto new_mp = AllocateAndConstructMountPoint(ref, mp);
							archetype.Destruction(mp);
							auto layout = archetype.GetSingleLayout();
							temp_resource.deallocate(mp.GetBuffer(0), layout.Size, layout.Align);
							ite.entity->mount_point = new_mp;
							ite.entity->status = EntityStatus::Normal;
						}
					}
				}

				for (auto& ite : removed_entity)
				{
					if (!ite.used)
					{
						CopyMountPointFormLast(components[ite.archetype_index], ite.mp);
					}
				}
				removed_entity.clear();
				spawned_entities.clear();
			}
		}

		return Updated;
	}

	ArchetypeMountPoint ArchetypeComponentManager::AllocateAndConstructMountPoint(Element& tar, ArchetypeMountPoint mp)
	{
		if(tar.last_page)
		{
			auto last = tar.last_page->GetLastMountPoint();
			if(last < tar.last_page->GetMaxMountPoint())
			{
				tar.archetype->MoveConstruct(last, mp);
				tar.last_page->available_count += 1;
				return last;
			}
		}

		auto page = ComponentPage::Create(tar.archetype->GetArchetypeLayout(), component_page_min_element_count, component_page_min_size, &components_resource);

		if(page)
		{
			auto first = page->begin();
			tar.archetype->MoveConstruct(first, mp);
			page->available_count += 1;
			if(!tar.top_page)
			{
				tar.top_page = page;
			}
			if(!tar.last_page)
			{
				tar.last_page = page;
			}else
			{
				tar.last_page->next_page = page;
				tar.last_page = page;
			}
			return first;
		}
		return {};
	}

	void ArchetypeComponentManager::CopyMountPointFormLast(Element& tar, ArchetypeMountPoint mp)
	{
		assert(tar.last_page);
		auto last = tar.last_page->end();
		assert(last.index != 0);
		last.index -= 1;
		if(last != mp)
		{
			tar.archetype->MoveConstruct(mp, last);
			tar.archetype->Destruction(last);
		}
		tar.last_page->available_count -= 1;
		if(tar.last_page->available_count == 0)
		{
			assert(tar.top_page);
			if(tar.top_page == tar.last_page)
			{
				tar.top_page.Reset();
				tar.last_page.Reset();
			}else
			{
				auto cur = tar.top_page;
				while(cur)
				{
					if(cur->next_page == tar.last_page)
					{
						cur->next_page.Reset();
						tar.last_page = std::move(cur);
						break;
					}
				}
			}
		}
	}

	/*
	bool ArchetypeComponentManager::UpdateEntityStatus()
	{

		std::lock_guard lg(components_mutex);




		std::lock_guard lg(spawn_mutex);
		if (need_update)
		{
			need_update = false;
			std::lock_guard lg2(components_mutex);
			while (!removed_entities.empty())
			{
				auto [arc, mp, status] = std::move(*removed_entities.rbegin());

				removed_entities.pop_back();
				assert(arc);

				auto location = arc->LocateTypeID(EntityPropertyArchetypeID().id);

				assert(location.has_value());

				auto top = static_cast<EntityProperty*>(arc->GetData(location->index, 0, mp))->entity;

				assert(top);

				std::lock_guard lg3(top->mutex);
				if (top->archetype)
				{
					assert(top->status == EntityStatus::PendingDestroy);
					if (status == EntityStatus::PreInit)
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
					}
					else
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
							[=](Entity::Ptr const& E)
							{
								assert(E && E->archetype);
								auto Tfi = E->archetype->GetFastIndex();
								return fi == Tfi;
							}
						);

						if (find == spawned_entities.end())
						{
							auto& ref = components[fi - 1];
							assert(ref.last_page);
							auto mp = ref.last_page->end();
							mp -= 1;
							if (old_mp != mp)
							{
								art->MoveConstruct(
									old_mp, mp
								);
								art->Destruction(mp);
							}

							ref.last_page->available_count -= 1;
							if (ref.last_page->available_count == 0)
							{
								assert(ref.top_page);
								if (ref.top_page == ref.last_page)
								{
									ref.top_page.Reset();
									ref.last_page.Reset();
								}
								else
								{
									auto last_page = ref.top_page;
									while (true)
									{
										if (!last_page->next_page)
										{
											assert(last_page->available_count == 0);
											ref.last_page = std::move(last_page);
											break;
										}
										else
										{
											last_page = last_page->next_page;
										}
									}
								}
							}
						}
						else
						{
							auto pre_init = std::move(*find);
							std::swap(*find, *spawned_entities.rbegin());
							spawned_entities.pop_back();
							assert(*art <=> *pre_init->archetype == std::strong_ordering::equivalent);
							for (std::size_t i = 0; i < art->GetTypeIDCount(); ++i)
							{
								art->MoveConstruct(old_mp, pre_init->mount_point);
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
			while (!spawned_entities.empty())
			{
				auto top = std::move(*spawned_entities.rbegin());
				spawned_entities.pop_back();
				assert(top->archetype && top->mount_point);
				auto fi = top->archetype->GetFastIndex();
				assert(fi <= cache_size);
				if (fi == 0)
				{
					auto cur_span = std::span(components).subspan(cache_size);
					std::size_t i = 0;
					for (auto& ite : cur_span)
					{
						if (*ite.archetype <=> *top->archetype == std::strong_ordering::equivalent)
						{
							fi = i + cache_size + 1;
							break;
						}
						++i;
					}
					if (fi == 0)
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
				if (ref.last_page)
				{
					auto last = ref.last_page->GetLastMountPoint();
					auto max = ref.last_page->GetMaxMountPoint();
					if (max == last)
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
				}
				else
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

				art->MoveConstruct(
					mp,
					top->mount_point
				);
				art->Destruction(
					top->mount_point
				);

				ref.last_page->available_count += 1;

				top->archetype = std::move(art);
				top->mount_point = mp;
				top->status = EntityStatus::Normal;
			}

			if (cache_size < components.size())
			{
				std::lock_guard lg3(filter_mapping_mutex);
				for (auto& ite : filter_mapping)
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
	*/

	/*
	bool ArchetypeComponentManager::ForeachMountPoint(std::size_t element_index, bool(*func)(void*, ArchetypeMountPointRange), void* data) const
	{
		std::shared_lock sl(components_mutex);
		if (element_index < components.size())
		{
			auto& ite = components[element_index];

			auto top = ite.top_page;

			while (top)
			{
				ArchetypeMountPointRange range{
					*ite.archetype, top->begin(), top->end()
				};
				if(!func(data, range))
					return false;
				top = top->GetNextPage();
			}
			return true;
		}
		return false;
	}

	bool ArchetypeComponentManager::ForeachMountPoint(std::size_t element_index, bool(*detect)(void*, Archetype const&), void* data, bool(*func)(void*, ArchetypeMountPointRange), void* data2) const
	{
		std::shared_lock sl(components_mutex);
		if (element_index < components.size())
		{
			auto& ite = components[element_index];

			if(!detect(data, *ite.archetype))
			{
				return true;
			}

			auto top = ite.top_page;

			while (top)
			{
				ArchetypeMountPointRange range{
					*ite.archetype, top->begin(), top->end()
				};
				if (!func(data, range))
					return false;
				top = top->GetNextPage();
			}
			return true;
		}
		return false;
	}

	bool ArchetypeComponentManager::ReadEntityMountPoint(Entity const& entity, void(*func)(void*, EntityStatus, Archetype const&, ArchetypeMountPoint), void* data) const
	{
		std::shared_lock sl(components_mutex);
		std::shared_lock sl2(entity.mutex);
		if(entity.resource == entity_resource->get_resource_interface() && entity.status != EntityStatus::Destroy)
		{
			assert(entity.archetype);
			func(data, entity.status, *entity.archetype, entity.mount_point);
			return true;
		}
		return false;
	}

	/*
	EntityPtr ArchetypeComponentManager::CreateEntityImp(EntityConstructor& constructor)
	{

		if(constructor.archetype_ptr && constructor.mount_point)
		{
			assert(constructor.archetype_ptr->GetTypeID(constructor.entity_property_index) == EntityPropertyArchetypeID().id);

			for(auto& ite : constructor.construct_record)
			{
				constructor.archetype_ptr->DefaultConstruct(ite.index,
					constructor.archetype_ptr->GetData(ite.index, ite.count, constructor.mount_point)
				);
			}

			assert(entity_resource);
			EntityPtr entity = Entity::Create(entity_resource->get_resource_interface());

			if (entity)
			{
				EntityProperty pro{
					entity
				};
				constructor.archetype_ptr->MoveConstruct(
					constructor.entity_property_index, 
					constructor.archetype_ptr->GetData(constructor.entity_property_index, 0, constructor.mount_point),
					&pro
				);

				{
					std::shared_lock sl(components_mutex);
					std::size_t index = 0;
					for (auto& ite : components)
					{
						assert(ite.archetype);
						if ((*ite.archetype) == (*constructor.archetype_ptr))
						{
							constructor.archetype_ptr->fast_index = index + 1;
						}
						++index;
					}
				}
				entity->archetype = std::move(constructor.archetype_ptr);
				entity->mount_point = constructor.mount_point;
				std::lock_guard lg(spawn_mutex);
				spawned_entities.push_back(entity);
				need_update = true;
				return entity;
			}
		}
		return {};
	}
	*/

	/*

	void ArchetypeComponentManager::ReleaseEntity(Entity& storage)
	{
		assert(storage.archetype && storage.mount_point);
		storage.archetype->Destruction(storage.mount_point);
		storage.mount_point = {};
		storage.archetype = {};
	}

	bool ArchetypeComponentManager::DestroyEntity(Entity& entity)
	{
		std::lock_guard lg(entity.mutex);
		if (entity.resource == entity_resource->get_resource_interface())
		{
			if (
				entity.status != EntityStatus::PendingDestroy
				|| entity.status != EntityStatus::Destroy
				)
			{
				auto last_status = entity.status;
				std::lock_guard lg(spawn_mutex);
				removed_entities.push_back({
					entity.archetype,
					entity.mount_point,
					entity.status
				});
				entity.status = EntityStatus::PendingDestroy;
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

	std::size_t ArchetypeComponentManager::ArchetypeCount() const
	{
		std::shared_lock sl(components_mutex);
		return components.size();
	}

	ArchetypeID const& ArchetypeComponentManager::EntityPropertyArchetypeID()
	{
		static ArchetypeID id = ArchetypeID::Create<EntityProperty>();
		return id;
	}
	*/

	/*
	std::optional<ArchetypeMountPointRange> ArchetypeComponentManager::GetArchetypeMountPointRange(std::size_t element_index) const
	{
		std::shared_lock sl(components_mutex);
		if(components.size() > element_index)
		{
			auto& ref = components[element_index];
			if(ref.top_page)
			{
				return ArchetypeMountPointRange{
					*ref.archetype,
					ref.top_page->begin(),
					ref.top_page->end()
				};
			}else
			{
				return ArchetypeMountPointRange{
					*ref.archetype,
					{},
					{}
				};
			}
		}
		return std::nullopt;
	}

	std::optional<ArchetypeMountPointRange> ArchetypeComponentManager::GetEntityMountPointRange(EntityStorage const& storage) const
	{
		if(storage.resource == entity_resource->get_resource_interface())
		{
			std::shared_lock sl(components_mutex);
			if(storage.status != EntityStatus::Destroy)
			{
				auto start = storage.mount_point;
				auto end = storage.mount_point;
				++end;
				return ArchetypeMountPointRange{
				*storage.archetype,
				start,
				end
				};
			}
			
		}
		return std::nullopt;
	}
	*/


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