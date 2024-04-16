module;

#include <cassert>

module NoodlesComponent;

constexpr std::size_t component_page_min_element_count = 10;
constexpr std::size_t component_page_min_size = 4096;

constexpr std::size_t component_page_huge_multiply_max_size = component_page_min_size * 32;
constexpr std::size_t component_page_huge_multiply = 4;

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
		archetype_index = 0;
		data_or_mount_point_index = 0;
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
			OnUnregister();
			return true;
		}
		return false;
	}

	void ComponentFilterInterface::OnUnregister()
	{
		indexs.clear();
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
		std::shared_lock lg(filter_mutex);
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
		std::shared_lock lg(filter_mutex);
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

	void SingletonFilterInterface::OnUnregister()
	{
		singleton_reference.Reset();
	}

	void* SingletonFilterInterface::GetSingleton(std::size_t in_owner_id) const
	{
		std::lock_guard lg(filter_mutex);
		if(owner_id != 0 && owner_id == in_owner_id && singleton_reference)
		{
			return singleton_reference->Get();
		}
		return nullptr;
	}

	std::tuple<Archetype::OPtr, Archetype::ArrayMountPoint> ArchetypeComponentManager::GetComponentPage(std::size_t archetype_index) const
	{
		std::shared_lock sl(component_mutex);
		if(archetype_index < components.size())
		{
			auto& ref = components[archetype_index];
			if(ref.memory_page)
			{
				return { ref.archetype.GetPointer(), ref.memory_page->GetMountPoint() };
			}else
			{
				return { ref.archetype.GetPointer(), {} };
			}
		}
		return {{}, {}};
	}

	ArchetypeComponentManager::EntityWrapper ArchetypeComponentManager::ReadEntity(Entity const& entity, ComponentFilterInterface const& interface, std::span<std::size_t> output_index) const
	{
		std::size_t archetype_index = 0;
		bool is_index = false;
		std::size_t data_or_index = 0;
		{
			std::shared_lock sl(entity.mutex);
			if (entity.owner_id != reinterpret_cast<std::size_t>(this))
			{
				return {};
			}
			if (entity.status == EntityStatus::Free)
				return {};
			archetype_index = entity.archetype_index;
			data_or_index = entity.data_or_mount_point_index;
			is_index = (entity.status == EntityStatus::Normal || entity.status == EntityStatus::PendingDestroy);
		}
		auto re = interface.EnumByArchetypeIndex(reinterpret_cast<std::size_t>(this), archetype_index, output_index);
		if(re)
		{
			auto [ar, mp] = GetComponentPage(archetype_index);
			if(ar)
			{
				if(is_index)
				{
					return { {ar, mp, output_index.subspan(0, *re)}, data_or_index };
				}else
				{
					mp.archetype_array_buffer = reinterpret_cast<void*>(data_or_index);
					mp.total_count = 1;
					mp.available_count = 1;
					return { { ar, mp, output_index.subspan(0, *re) },  0 };
				};
			}
		}
		return { {}, 0 };
	}


	ArchetypeComponentManager::ArchetypeComponentManager(SyncResource resource)
		:components(resource.manager_resource), spawned_entities(resource.manager_resource), temp_resource(resource.manager_resource),
		archetype_resource(resource.archetype_resource),components_resource(resource.component_resource), singletons(resource.manager_resource),
		filter_mapping(resource.manager_resource), singleton_filters(resource.manager_resource), singleton_resource(resource.singleton_resource)
	{
		
	}

	
	ArchetypeComponentManager::~ArchetypeComponentManager()
	{
		{
			std::lock_guard lg(filter_mapping_mutex);
			for(auto& ite : filter_mapping)
			{
				auto& ref = *ite.filter;
				ref.Unregister(reinterpret_cast<size_t>(this));
			}
			filter_mapping.clear();
			for(auto& ite : singleton_filters)
			{
				auto& ref = *ite.ptr;
				ref.Unregister(reinterpret_cast<size_t>(this));
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
					void* buffer = reinterpret_cast<void*>(ref.data_or_mount_point_index);
					assert(ref.archetype_index < components.size());
					auto& ref2 = components[ref.archetype_index];

					for (auto& it3 : *ref2.archetype)
					{
						ref2.archetype->Destruct(Archetype::ArrayMountPoint{
							buffer, 1, 1
							}, 0
						);
					}

					auto slayout = ref2.archetype->GetSingleLayout();
					temp_resource->deallocate(buffer, slayout.Size, slayout.Align);
					ref.SetFree();
				}
			}
		}

		{
			std::lock_guard lg2(component_mutex);
			for(auto& ite : components)
			{
				auto ite2 = ite.memory_page;
				if(ite.memory_page)
				{
					auto mp = ite.memory_page->GetMountPoint();
					auto index = ite.memory_page->available_count;
					auto entity_property = ite.archetype->Get(ite.entity_property_locate, mp).Translate<EntityProperty>();
					for(auto& ite : entity_property)
					{
						auto entity = ite.GetEntity();
						assert(entity);
						std::lock_guard lg(entity->mutex);
						entity->SetFree();
					}
					for (auto& ite3 : *ite.archetype)
					{
						auto array_list = ite.archetype->Get(ite3, ite2->GetMountPoint());
						for(std::size_t i = 0; i < index; ++i)
							ite.archetype->Destruct(ite3, array_list, i);
					}
				}
				
				ite2.Reset();
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

	std::tuple<Archetype::Ptr, std::size_t, Archetype::MountPoint> ArchetypeComponentManager::CreateArchetype(std::span<ArchetypeID const> ids, std::span<std::size_t> output)
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
				std::pmr::vector<ArchetypeID> temp_ids(temp_resource);
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
					auto loc = archetype_ptr->LocateTypeID(UniqueTypeID::Create<EntityProperty>());
					assert(loc);
					components.emplace_back(
						archetype_ptr,
						ComponentPage::Ptr{},
						*loc,
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
			auto mp = Potato::IR::MemoryResourceRecord::Allocate(temp_resource, archetype_ptr->GetSingleLayout());
			return { archetype_ptr, archetype_index, {
				{mp.Get(), 1, 1},
				0
			}};
		}

		return { {}, 0, {} };
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

	ArchetypeComponentManager::ComponentsWrapper ArchetypeComponentManager::ReadComponents(ComponentFilterInterface const& interface, std::size_t ite_index, std::span<std::size_t> output_span) const
	{
		std::size_t archetype_index = std::numeric_limits<std::size_t>::max();
		auto re = interface.EnumByIteratorIndex(reinterpret_cast<std::size_t>(this), ite_index, archetype_index, output_span);
		if(re)
		{
			auto [re2, mp] = GetComponentPage(archetype_index);
			if(re2)
			{
				return {re2.GetPointer(), mp, output_span.subspan(0, *re) };
			}
		}
		return {{}, {}, {}};
	}

	bool ArchetypeComponentManager::RegisterFilter(SingletonFilterInterface::Ptr ptr, std::size_t group_id)
	{
		if(ptr && ptr->Register(reinterpret_cast<std::size_t>(this)))
		{
			{
				std::lock_guard lg(ptr->filter_mutex);
				ptr->singleton_reference.Reset();
			}
			
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
			std::lock_guard lg3(filter_mapping_mutex);
			singleton_filters.emplace_back(std::move(ptr), RID, group_id);
			return true;
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
							ptr->status = EntityStatus::PendingDestroyWithoutInit;
							return true;
						}
					}
				}
				break;
			case EntityStatus::Normal:
			{
				std::lock_guard lg(spawn_mutex);
				spawned_entities.emplace_back(
					ptr, SpawnedStatus::RemoveOld, ptr->archetype_index, true
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

	Potato::Pointer::ObserverPtr<void> ArchetypeComponentManager::ReadSingleton(SingletonFilterInterface const& filter) const
	{
		return filter.GetSingleton(reinterpret_cast<std::size_t>(this));
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

				std::lock_guard lg3(spawn_mutex);

				for(auto ite = spawned_entities.begin(); ite != spawned_entities.end(); ++ite)
				{
					if(ite->need_handle)
					{
						ite->need_handle = false;
						auto& ref = components[ite->archetype_index];
						assert(ref.archetype);
						std::lock_guard lg(ite->entity->mutex);
						void* buffer = reinterpret_cast<void*>(ite->entity->data_or_mount_point_index);
						switch (ite->status)
						{
						case SpawnedStatus::New:
						{
							bool done = false;
							for(auto ite2 = ite + 1; ite2 != spawned_entities.end(); ++ite2)
							{
								if(ite2->status == SpawnedStatus::RemoveOld && ite2->archetype_index == ite->archetype_index)
								{
									assert(ref.memory_page);
									done = true;
									ite2->need_handle = false;
									assert(ite->archetype_index < components.size());
									auto rmp = ref.memory_page->GetMountPoint();
									assert(ite2->entity);
									std::lock_guard lg(ite2->entity->mutex);
									ref.archetype->Destruct(rmp, ite2->entity->data_or_mount_point_index);
									for(auto& it3 : *ref.archetype)
									{
										ref.archetype->MoveConstruct(it3, 
											ref.archetype->Get(it3, rmp, ite2->entity->data_or_mount_point_index), 
											buffer
										);
									}
									ite->entity->data_or_mount_point_index = ite2->entity->data_or_mount_point_index;
									ite->entity->status = EntityStatus::Normal;
									ite2->entity->SetFree();
								}
							}
								if(!done)
								{
									auto mp = AllocateAndConstructMountPoint(ref, Archetype::ArrayMountPoint{
										buffer, 1, 1
										}, 0
									);
									assert(mp);
									ite->entity->data_or_mount_point_index = *mp;
									ite->entity->status = EntityStatus::Normal;
								}
								for (auto& it3 : *ref.archetype)
								{
									ref.archetype->Destruct(Archetype::ArrayMountPoint{
										buffer, 1, 1
										}, 0
									);
								}
								auto slayout = ref.archetype->GetSingleLayout();
								temp_resource->deallocate(buffer, slayout.Size, slayout.Align);
						}break;
						case SpawnedStatus::NewButNeedRemove:
						{
							for (auto& it3 : *ref.archetype)
							{
								ref.archetype->Destruct(Archetype::ArrayMountPoint{
									buffer, 1, 1
									}, 0
								);
							}
							auto slayout = ref.archetype->GetSingleLayout();
							temp_resource->deallocate(buffer, slayout.Size, slayout.Align);
							ite->entity->SetFree();
						}break;
						case SpawnedStatus::RemoveOld:
						{
							assert(ref.memory_page);
							CopyMountPointFormLast(ref, ref.memory_page->GetMountPoint(), ite->entity->data_or_mount_point_index);
							ite->entity->SetFree();
						}break;
						}
					}
				}
				spawned_entities.clear();
			}
		}

		return Updated;
	}

	std::optional<std::size_t> ArchetypeComponentManager::AllocateAndConstructMountPoint(Element& tar, Archetype::ArrayMountPoint mp, std::size_t mp_target)
	{

		if(!tar.memory_page)
		{
			tar.memory_page = ComponentPage::Create(tar.archetype->GetArchetypeLayout(), component_page_min_element_count, component_page_min_size, &components_resource);
		}

		if(tar.memory_page)
		{
			if (tar.memory_page->available_count >= tar.memory_page->max_element_count)
			{
				std::size_t target_space = tar.memory_page->allocate_size;
				if (target_space < component_page_huge_multiply_max_size)
				{
					target_space *= component_page_huge_multiply;
				}else
				{
					target_space *= 2;
				}

				auto new_page = ComponentPage::Create(tar.archetype->GetArchetypeLayout(), tar.memory_page->max_element_count, target_space, &components_resource);

				if(new_page)
				{
					auto t1 = new_page->GetMountPoint();
					new_page->available_count = tar.memory_page->available_count;
					auto t2 = tar.memory_page->GetMountPoint();
					auto last_index = t2.available_count;
					for(auto& ite : *tar.archetype)
					{
						auto ar1 = tar.archetype->Get(ite, t1);
						auto ar2 = tar.archetype->Get(ite, t2);
						for (std::size_t i2 = 0; i2 < last_index; ++i2)
						{
							tar.archetype->MoveConstruct(ite, ar1, i2, ar1, i2);
							tar.archetype->Destruct(ite, ar1, i2);
						}
					}
					new_page->available_count = tar.memory_page->available_count;
					tar.memory_page = std::move(new_page);
				}else
				{
					return std::nullopt;
				}
			}
		}

		if(tar.memory_page)
		{
			tar.memory_page->available_count += 1;
			auto nmp = tar.memory_page->GetMountPoint();
			auto next_index = nmp.available_count - 1;
			tar.archetype->MoveConstruct(nmp, next_index, mp, mp_target);
			return next_index;
		}

		return std::nullopt;
	}

	void ArchetypeComponentManager::CopyMountPointFormLast(Element& tar, Archetype::ArrayMountPoint mp, std::size_t mp_index)
	{
		auto last = tar.memory_page->GetMountPoint();
		assert(last == mp && last.available_count > mp_index);
		last.available_count -= 1;
		if(last.available_count != mp_index)
		{
			auto pro = static_cast<EntityProperty*>(tar.archetype->Get(tar.entity_property_locate, last, last.available_count));
			auto tar_entity = pro->GetEntity();
			tar.archetype->MoveConstruct(mp, mp_index, last, last.available_count);
			tar.archetype->Destruct(last, last.available_count);
			std::lock_guard lg(tar_entity->mutex);
			tar_entity->data_or_mount_point_index = mp_index;
		}
	}

	std::size_t ArchetypeComponentManager::ReleaseFilter(std::size_t group_id)
	{
		std::lock_guard lg(filter_mapping_mutex);
		auto id = reinterpret_cast<std::size_t>(this);
		auto osize = filter_mapping.size() + singleton_filters.size();
		filter_mapping.erase(
			std::remove_if(filter_mapping.begin(), filter_mapping.end(),
				[=](CompFilterElement& ref)-> bool
				{
					if(ref.group_id == group_id)
					{
						ref.filter->Unregister(id);
						ref.filter.Reset();
						return true;
					}
					return false;
				}
			),
			filter_mapping.end()
		);

		singleton_filters.erase(
			std::remove_if(singleton_filters.begin(), singleton_filters.end(),
				[=](SingletonFilterElement& ref)-> bool
				{
					if (ref.group_id == group_id)
					{
						ref.ptr->Unregister(id);
						ref.ptr.Reset();
						return true;
					}
					return false;
				}
			),
			singleton_filters.end()
		);

		return osize - filter_mapping.size() - singleton_filters.size();
	}
}