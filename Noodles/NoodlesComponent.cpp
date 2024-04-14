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

	std::tuple<Archetype::OPtr, ArchetypeMountPoint, std::span<std::size_t>> ArchetypeComponentManager::ReadEntity(Entity const& entity, ComponentFilterInterface const& interface, std::span<std::size_t> output_index) const
	{
		Archetype::OPtr arc;
		std::size_t archetype_index;
		ArchetypeMountPoint mp;
		{
			std::shared_lock sl(entity.mutex);
			if (entity.owner_id != reinterpret_cast<std::size_t>(this))
			{
				return { {}, {}, {} };
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
				return {arc, mp, output_index.subspan(0, *re)};
			}
		}
		return {{}, {}, {}};
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

					ref.archetype->Destruct(ref.mount_point);
					auto layout = ref.archetype->GetSingleLayout();
					temp_resource.deallocate(ref.mount_point.GetBuffer(), layout.Size, layout.Size);
					ref.SetFree();
				}
			}
		}

		{
			std::lock_guard lg2(component_mutex);
			for(auto& ite : components)
			{
				auto ite2 = ite.memory_page;
				for(auto& ite3 : *ite.archetype)
				{
					for(auto ite4 : *ite2)
					{
						ite.archetype->Destruct(ite3, ite4);
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

	std::tuple<Archetype::OPtr, ArchetypeMountPoint, ArchetypeMountPoint, std::span<std::size_t>> ArchetypeComponentManager::ReadComponents(ComponentFilterInterface const& interface, std::size_t ite_index, std::span<std::size_t> output_span) const
	{
		std::lock_guard lg(component_mutex);
		std::size_t archetype_index = std::numeric_limits<std::size_t>::max();
		auto re = interface.EnumByIteratorIndex(reinterpret_cast<std::size_t>(this), ite_index, archetype_index, output_span);
		if(re)
		{
			assert(components.size() > archetype_index);
			auto& ref = components[archetype_index];
			if(ref.memory_page)
			{
				return { ref.archetype, ref.memory_page->begin(), ref.memory_page->end(), output_span.subspan(0, *re)};
			}else
			{
				return { ref.archetype, ArchetypeMountPoint{}, ArchetypeMountPoint{}, output_span.subspan(0, *re) };
			}
		}
		return {{}, {}, {}, {}};
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
						ite.entity->archetype->Destruct(
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
								archetype.Destruct(mp);
								auto layout = archetype.GetSingleLayout();
								temp_resource.deallocate(mp.GetBuffer(), layout.Size, layout.Align);
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
							archetype.Destruct(mp);
							auto layout = archetype.GetSingleLayout();
							temp_resource.deallocate(mp.GetBuffer(), layout.Size, layout.Align);
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
					auto t1 = new_page->begin();
					auto t2 = tar.memory_page->begin();
					auto last_index = tar.memory_page->available_count;
					for(auto& ite : *tar.archetype)
					{
						for (std::size_t i2 = 0; i2 < last_index; ++i2)
						{
							t1.index = i2;
							t2.index = i2;
							tar.archetype->MoveConstruct(ite, t1, t2);
							tar.archetype->Destruct(ite, t2);
						}
					}
					new_page->available_count = tar.memory_page->available_count;
					tar.memory_page = std::move(new_page);
				}else
				{
					return {};
				}
			}
		}

		if(tar.memory_page)
		{
			auto nmp = tar.memory_page->end();
			tar.archetype->MoveConstruct(nmp, mp);
			tar.memory_page->available_count += 1;
			return mp;
		}

		return {};
	}

	void ArchetypeComponentManager::CopyMountPointFormLast(Element& tar, ArchetypeMountPoint mp)
	{
		assert(tar.memory_page && tar.memory_page->begin().buffer == mp.buffer);
		auto last = tar.memory_page->GetLastMountPoint();
		if(mp + 1 != last)
		{
			last.index -= 1;
			tar.archetype->MoveConstruct(mp, last);
			tar.archetype->Destruct(last);
			tar.memory_page->available_count -= 1;
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