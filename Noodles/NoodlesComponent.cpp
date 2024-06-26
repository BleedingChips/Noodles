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

	EntityModifer::Ptr EntityModifer::Create(std::pmr::memory_resource* resource)
	{
		auto re = Potato::IR::MemoryResourceRecord::Allocate<EntityModifer>(resource);
		if(re)
		{
			return new (re.Get()) EntityModifer{re};
		}
		return {};
	}

	void EntityModifer::Release()
	{
		auto re = record;
		this->~EntityModifer();
		re.Deallocate();
	}

	EntityModifer::~EntityModifer()
	{
		for(auto& ite : datas)
		{
			if(ite.record.Get() != nullptr)
			{
				(*ite.id.wrapper_function)(ArchetypeID::Status::Destruction, ite.record.Get(), nullptr);
				ite.record.Deallocate();
			}
		}
		datas.clear();
	}

	void Entity::SetFree()
	{
		status = EntityStatus::Free;
		archetype_index = std::numeric_limits<std::size_t>::max();
		mount_point_index = 0;
		owner_id = 0;
		archetype_index = std::numeric_limits<std::size_t>::max();
		modifer = {};
		need_modifier = false;
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
	/*
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
	*/

	void ComponentFilter::OnCreatedArchetype(std::size_t archetype_index, Archetype const& archetype)
	{
		std::lock_guard lg(mutex);
		auto aspan = GetArchetypeId();
		assert(aspan.size() > 0);
		auto old_size = index.size();
		index.resize(old_size + aspan.size() + 1);
		auto out_span = std::span(index).subspan(old_size);
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
				index.resize(old_size);
				return;
			}
		}
	}

	std::optional<std::span<std::size_t>> ComponentFilter::EnumMountPointIndexByArchetypeIndex_AssumedLocked(std::size_t archetype_index, std::span<std::size_t> output) const
	{
		auto size = GetArchetypeId().size();
		if(output.size() >= size)
		{
			auto span = std::span(index);
			assert((span.size() % (size + 1)) == 0);
			while(!span.empty())
			{
				if(span[0] == archetype_index)
				{
					span = span.subspan(1, size);
					std::memcpy(output.data(), span.data(), span.size() * sizeof(std::size_t));
					return output.subspan(0, size);
				}else
				{
					span = span.subspan(size + 1);
				}
			}
		}
		return std::nullopt;
	}

	std::optional<std::span<std::size_t>> ComponentFilter::EnumMountPointIndexByIterator_AssumedLocked(std::size_t ite_index, std::size_t& archetype_index, std::span<std::size_t> output) const
	{
		auto size = GetArchetypeId().size();
		if(output.size() >= size)
		{
			auto span = std::span(index);
			assert((span.size() % (size + 1)) == 0);

			auto offset = ite_index * (size + 1);
			if(offset < span.size())
			{
				span = span.subspan(offset);
				archetype_index = span[0];
				span = span.subspan(1, size);
				std::memcpy(output.data(), span.data(), span.size() * sizeof(std::size_t));
				return output.subspan(0, size);
			}
		}
		
		return std::nullopt;
	}

	void ComponentFilter::WeakRelease()
	{
		auto re = record;
		this->~ComponentFilter();
		re.Deallocate();
	}

	/*
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
	*/

	std::optional<std::span<void*>> ArchetypeComponentManager::ReadEntityDirect_AssumedLocked(Entity const& entity, ComponentFilter const& filter, std::span<void*> output_ptr, bool prefer_modify) const
	{
		std::shared_lock lg(entity.mutex);
		std::shared_lock lg2(filter.mutex);
		if(entity.owner_id == reinterpret_cast<std::size_t>(this) && filter.owner.GetPointer() == this)
		{
			auto span = filter.GetArchetypeId();
			prefer_modify = entity.modifer && prefer_modify;
			if(output_ptr.size() >= span.size())
			{
				std::size_t span_index = 0;
				auto [mp, index] = GetComponentPage(entity.archetype_index);
				for(auto& ite : span)
				{
					bool find = false;
					void* data = nullptr;
					if(prefer_modify)
					{
						for(auto& ite2 : entity.modifer->datas)
						{
							if(ite2.available && ite2.id.id == ite)
							{
								find = true;
								if(ite2.record)
								{
									data = ite2.record.Get();
								}
								break;
							}
						}
					}
					if(data == nullptr && find)
					{
						if(mp)
						{
							auto loc = mp->LocateTypeID(ite);
							if(loc)
							{
								data = mp->Get(*loc, index, entity.mount_point_index);
							}
						}
					}
					output_ptr[span_index++] = data;
				}
				return output_ptr.subspan(0, span.size());
			}
		}
		return std::nullopt;
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

	ComponentFilter::SPtr ArchetypeComponentManager::CreateComponentFilter(std::span<UniqueTypeID const> require_component)
	{
		std::size_t require_hash = 0;
		for(std::size_t i = 0; i < require_component.size(); ++i)
		{
			require_hash += require_component[i].HashCode() + i * i;
		}
		std::lock_guard lg(filter_mutex);
		for(auto& ite : component_filter)
		{
			auto k = ite.Isomer();
			if(k && k->GetHash() == require_hash)
			{
				auto span = k->GetArchetypeId();
				if(span.size() == require_component.size())
				{
					bool Equal = true;
					for(std::size_t i = 0; i < require_component.size(); ++i)
					{
						if(span[i] != require_component[i])
						{
							Equal = false;
							break;
						}
					}
					if(Equal)
						return k;
				}
			}
		}
		auto layout = Potato::IR::Layout::Get<ComponentFilter>();
		auto offset = Potato::IR::InsertLayoutCPP(layout, Potato::IR::Layout::GetArray<UniqueTypeID>(require_component.size()));
		Potato::IR::FixLayoutCPP(layout);
		auto re = Potato::IR::MemoryResourceRecord::Allocate(filter_resource, layout);
		if(re)
		{
			auto span =	std::span(reinterpret_cast<UniqueTypeID*>(re.GetByte() + offset), require_component.size());
			for(std::size_t i = 0; i < require_component.size(); ++i)
			{
				new (&span[i]) UniqueTypeID{require_component[i]};
			}
			ComponentFilter::SPtr ptr = new(re.Get()) ComponentFilter{
				re, require_hash, span, this
			};
			std::shared_lock sl(component_mutex);
			for(std::size_t i = 0; i < components.size(); ++i)
			{
				ptr->OnCreatedArchetype(i, *components[i].archetype);
			}
			component_filter.emplace_back(ptr.Isomer());
			return ptr;
		}
		return {};
	}

	ArchetypeComponentManager::ArchetypeComponentManager(SyncResource resource)
		:components(resource.manager_resource), modified_entity(resource.manager_resource), temp_resource(resource.temporary_resource),
		archetype_resource(resource.archetype_resource),components_resource(resource.component_resource), singletons(resource.manager_resource),

		filter_resource(resource.filter_resource),component_filter(resource.manager_resource), singleton_resource(resource.singleton_resource)
	{
		
	}

	
	ArchetypeComponentManager::~ArchetypeComponentManager()
	{
		/*
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
		*/

		{
			std::lock_guard lg2(singletons_mutex);
			singletons.clear();
		}

		{
			std::lock_guard lg(entity_modifier_mutex);
			for(auto& ite : modified_entity)
			{
				if(ite)
				{
					std::lock_guard lg(ite->mutex);
					ite->SetFree();
				}
			}
			modified_entity.clear();
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

	bool ArchetypeComponentManager::CheckIsSameArchetype(Archetype const& target, std::size_t hash_code, std::span<ArchetypeID const> ids)
	{
		if(target.GetHashCode() == hash_code && target.GetElementCount() == ids.size())
		{
			for (auto& ite2 : ids)
			{
				auto loc = target.LocateTypeID(ite2.id);
				if (!loc.has_value())
				{
					return false;
				}
			}
			return true;
		}
		return false;
	}

	EntityPtr ArchetypeComponentManager::CreateEntity(std::pmr::memory_resource* entity_resource)
	{
		auto ent = Entity::Create(entity_resource);
		if(ent)
		{
			ent->modifer = EntityModifer::Create(temp_resource);
			if(ent->modifer)
			{
				auto re2 = Potato::IR::MemoryResourceRecord::Allocate<EntityProperty>(temp_resource);
				if(re2)
				{
					new (re2.Get()) EntityProperty {ent};
					ent->modifer->datas.emplace_back(
						true,
						ArchetypeID::Create<EntityProperty>(),
						re2
					);
					ent->owner_id = reinterpret_cast<std::size_t>(this);
					ent->status = EntityStatus::PreInit;
					ent->need_modifier = true;
					std::lock_guard lg(entity_modifier_mutex);
					modified_entity.emplace_back(ent);
					return ent;
				}
			}
		}
		return {};
	}


	ArchetypeComponentManager::ComponentsWrapper ArchetypeComponentManager::ReadComponents_AssumedLocked(ComponentFilter const& filter, std::size_t ite_index, std::span<std::size_t> output) const
	{
		std::shared_lock sl(filter.mutex);
		if(filter.owner.GetPointer() == this)
		{
			std::size_t archetype_index = std::numeric_limits<std::size_t>::max();
			auto re = filter.EnumMountPointIndexByIterator_AssumedLocked(ite_index, archetype_index, output);
			if(re)
			{
				auto [re2, mp] = GetComponentPage(archetype_index);
				if(re2)
				{
					return {re2.GetPointer(), mp, *re };
				}
			}
		}
		
		return {{}, {}, {}};
	}

	
	ArchetypeComponentManager::EntityWrapper ArchetypeComponentManager::ReadEntityComponents_AssumedLocked(Entity const& ent, ComponentFilter const& filter, std::span<std::size_t> output_span) const
	{
		std::shared_lock sl2(ent.mutex);
		std::shared_lock sl(filter.mutex);
		if(ent.owner_id == reinterpret_cast<std::size_t>(this) && filter.owner.GetPointer() == this)
		{
			auto re = filter.EnumMountPointIndexByArchetypeIndex_AssumedLocked(ent.archetype_index, output_span);
			if(re.has_value())
			{
				auto [re2, mp] = GetComponentPage(ent.archetype_index);
				if(re2)
				{
					return {{re2.GetPointer(), mp, *re }, ent.mount_point_index};
				}
			}
		}
		return {{}, 0};
	}

	bool ArchetypeComponentManager::ReleaseEntity(Entity& ptr)
	{
		bool need_insert_modifer = false;
		{
			std::lock_guard lg(ptr.mutex);
			if (ptr.owner_id != reinterpret_cast<std::size_t>(this))
				return false;
			switch(ptr.status)
			{
			case EntityStatus::PreInit:
				{
					ptr.status = EntityStatus::PendingDestroy;
				}
				break;
			case EntityStatus::Normal:
				{
					ptr.status = EntityStatus::PendingDestroy;
					break;
				}
			default:
				return false;
				break;
			}
			if(!ptr.need_modifier)
			{
				ptr.need_modifier = true;
				need_insert_modifer = true;
			}
		}
		if(need_insert_modifer)
		{
			std::lock_guard lg(entity_modifier_mutex);
			modified_entity.emplace_back(&ptr);
		}
		return true;
	}

	/*
	Potato::Pointer::ObserverPtr<void> ArchetypeComponentManager::ReadSingleton(SingletonFilterInterface const& filter) const
	{
		return filter.GetSingleton(reinterpret_cast<std::size_t>(this));
	}
	*/

	bool ArchetypeComponentManager::AddEntityComponent(Entity& target_entity, ArchetypeID archetype_id, void* reference_buffer)
	{
		if(archetype_id.id != UniqueTypeID::Create<EntityProperty>())
		{
			bool need_modifer = false;
			{
				std::lock_guard lg(target_entity.mutex);
				if(target_entity.owner_id != reinterpret_cast<std::size_t>(this))
					return false;
				bool has_modifier = target_entity.modifer;
				if(!has_modifier)
				{
					std::shared_lock sl(component_mutex);
					auto& ref = components[target_entity.archetype_index];
					auto lo = ref.archetype->LocateTypeID(archetype_id);
					if(lo)
					{
						return false;
					}
					target_entity.modifer = EntityModifer::Create(temp_resource);
					if(!target_entity.modifer)
						return false;
					for(auto& ite : *ref.archetype)
					{
						target_entity.modifer->datas.emplace_back(
							true,
							ite.id,
							Potato::IR::MemoryResourceRecord{}
						);
					}
					if(!target_entity.need_modifier)
					{
						target_entity.need_modifier = true;
						need_modifer = true;
					}
				}

				assert(target_entity.modifer);

				if(has_modifier)
				{
					for(auto& ite : target_entity.modifer->datas)
					{
						if(ite.id == archetype_id && ite.available)
						{
							return false;
						}
					}
				}
				
				auto re = Potato::IR::MemoryResourceRecord::Allocate(temp_resource, archetype_id.layout);
				if(re)
				{
					(*archetype_id.wrapper_function)(ArchetypeID::Status::MoveConstruction, re.Get(), reference_buffer);
					target_entity.modifer->datas.emplace_back(
						true,
							archetype_id,
					re
					);
				}else
				{
					if(need_modifer)
					{
						std::lock_guard lg(entity_modifier_mutex);
						modified_entity.emplace_back(&target_entity);
					}
					return false;
				}
			}
			if(need_modifer)
			{
				std::lock_guard lg(entity_modifier_mutex);
				modified_entity.emplace_back(&target_entity);
			}
			return true;
		}
		return false;
	}

	bool ArchetypeComponentManager::RemoveEntityComponent(Entity& target_entity, UniqueTypeID id)
	{
		if(id != UniqueTypeID::Create<EntityProperty>())
		{
			bool need_modifer = false;

			{
				std::lock_guard lg(target_entity.mutex);
				if(target_entity.owner_id == reinterpret_cast<std::size_t>(this))
				{
					if(!target_entity.modifer)
					{
						std::shared_lock sl(component_mutex);
						auto& ref = components[target_entity.archetype_index];
						auto lo = ref.archetype->LocateTypeID(id);
						if(!lo)
						{
							return false;
						}

						for(auto& ite : *ref.archetype)
						{
							target_entity.modifer->datas.emplace_back(
								ite.id.id != id,
								ite.id,
								Potato::IR::MemoryResourceRecord{}
							);
						}

						if(!target_entity.need_modifier)
						{
							target_entity.need_modifier = true;
							need_modifer = true;
						}
					}else
					{
						for(auto& ite : target_entity.modifer->datas)
						{
							if(ite.id.id == id && ite.available)
							{
								ite.available = false;
								return true;
							}
						}
						return false;
					}
				}else
				{
					return false;
				}
			}

			if(need_modifer)
			{
				std::lock_guard lg(entity_modifier_mutex);
				modified_entity.emplace_back(&target_entity);
			}
			return true;
		}
		return false;
	}

	bool ArchetypeComponentManager::ForceUpdateState()
	{
		bool Updated = false;

		{
			std::lock_guard lg(component_mutex);
			auto old_component_size = components.size();
			struct RemoveEntity
			{
				std::size_t archetype_index;
				std::size_t mount_point_index;
			};
			std::pmr::vector<RemoveEntity> remove_list(temp_resource);
			std::pmr::vector<ArchetypeID> new_archetype_index(temp_resource);
			while(true)
			{
				Entity::Ptr top;
				{
					std::lock_guard lg(entity_modifier_mutex);
					if(!modified_entity.empty())
					{
						top = std::move(*modified_entity.rbegin());
						modified_entity.pop_back();
					}
				}
				if(!top)
					break;
				Updated = true;
				top->need_modifier = false;
				std::lock_guard lg(top->mutex);
				switch(top->status)
				{
				case EntityStatus::PendingDestroy:
					{
						top->modifer.Reset();
						if(top->archetype_index != std::numeric_limits<std::size_t>::max())
						{
							auto& ref = components[top->archetype_index];
							ref.archetype->Destruct(ref.memory_page->GetMountPoint(), top->mount_point_index);
							remove_list.emplace_back(top->archetype_index, top->mount_point_index);
						}
						top->SetFree();
						break;
					}
				case EntityStatus::Normal:
				case EntityStatus::PreInit:
					{
						assert(top->modifer);
						new_archetype_index.clear();
						std::size_t hash_code = 0;
						for(auto& ite : top->modifer->datas)
						{
							if(ite.available)
							{
								new_archetype_index.emplace_back(ite.id);
								hash_code += ite.id.id.HashCode();
							}
						}
						Archetype::OPtr archetype_ptr;
						std::size_t archetype_index = 0;
						for(auto& ite : components)
						{
							if(CheckIsSameArchetype(*ite.archetype, hash_code, std::span(new_archetype_index)))
							{
								archetype_ptr = ite.archetype;
								break;
							}
							++archetype_index;
						}
						if(!archetype_ptr)
						{
							std::sort(new_archetype_index.begin(), new_archetype_index.end(), [](ArchetypeID const& a1, ArchetypeID const& a2)
								{
									return a1 > a2;
								});

							auto temp_ptr = Archetype::Create(new_archetype_index, &archetype_resource);

							if (temp_ptr)
							{
								archetype_ptr = temp_ptr;
								auto loc = temp_ptr->LocateTypeID(UniqueTypeID::Create<EntityProperty>());
								assert(loc);
								components.emplace_back(
									temp_ptr,
									ComponentPage::Ptr{},
									*loc,
									0
								);
							}
						}
						if(archetype_ptr)
						{
							auto& ref = components[archetype_index];

							auto find = std::find_if(remove_list.begin(), remove_list.end(), [=](RemoveEntity const& ent)
							{
								return ent.archetype_index == archetype_index;
							});

							std::optional<std::size_t> re;

							if(find != remove_list.end())
							{
								re = find->mount_point_index;
								remove_list.erase(find);
							}else
							{
								re = AllocateMountPoint(ref);
							}

							if(re)
							{
								auto mp = ref.memory_page->GetMountPoint();
								for(auto& ite : top->modifer->datas)
								{
									if(ite.available)
									{
										auto lo = archetype_ptr->LocateTypeID(ite.id);
										assert(lo);
										if(ite.record)
										{
											auto& ref2 = (*archetype_ptr)[*lo];
											archetype_ptr->MoveConstruct(
												ref2, archetype_ptr->Get(ref2, mp, *re), ite.record.Get()
											);
										}else
										{
											auto& ref2 = (*archetype_ptr)[*lo];
											auto& ref3 = components[top->archetype_index];
											auto lo2 = ref3.archetype->LocateTypeID(ite.id);
											auto mp2 = ref3.memory_page->GetMountPoint();
											archetype_ptr->MoveConstruct(
												ref2, archetype_ptr->Get(ref2, mp, *re),
												ref3.archetype->Get(*lo2, mp2, top->mount_point_index)
											);
										}
									}
								}
								if(top->status == EntityStatus::PreInit)
								{
									top->status = EntityStatus::Normal;
									
								}else
								{
									auto& ref = components[top->archetype_index];
									ref.archetype->Destruct(ref.memory_page->GetMountPoint(), top->mount_point_index);
									remove_list.emplace_back(archetype_index, *re);
								}
								top->archetype_index = archetype_index;
								top->mount_point_index = *re;
								top->modifer.Reset();
							}

						}

						break;
					}
				default:
					assert(false);
					break;
				}
			}

			for(auto& ite : remove_list)
			{
				auto& ref = components[ite.archetype_index];
				CopyMountPointFormLast(ref, ite.mount_point_index);
				Updated = true;
			}

			remove_list.clear();

			if(old_component_size != components.size())
			{
				std::lock_guard lg2(filter_mutex);

				component_filter.erase(
					std::remove_if(component_filter.begin(), component_filter.end(), [&](ComponentFilter::WPtr& ptr)->bool
					{
						auto up = ptr.Isomer();
						if(up)
						{
							for(std::size_t i = old_component_size; i < components.size(); ++i)
							{
								up->OnCreatedArchetype(i, *components[i].archetype);
							}
							return false;
						}else
						{
							return true;
						}
					}),
					component_filter.end()
				);
				Updated = true;
			}
		}

		return Updated;
	}

	std::optional<std::size_t> ArchetypeComponentManager::AllocateMountPoint(Element& tar)
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
					new_page->available_count = tar.memory_page->available_count;
					auto t1 = new_page->GetMountPoint();
					auto t2 = tar.memory_page->GetMountPoint();
					auto last_index = t2.available_count;
					for(auto& ite : *tar.archetype)
					{
						auto ar1 = tar.archetype->Get(ite, t1);
						auto ar2 = tar.archetype->Get(ite, t2);
						for (std::size_t i2 = 0; i2 < last_index; ++i2)
						{
							tar.archetype->MoveConstruct(ite, ar1, i2, ar2, i2);
							tar.archetype->Destruct(ite, ar2, i2);
						}
					}
					tar.memory_page = std::move(new_page);
				}else
				{
					return std::nullopt;
				}
			}
		}

		if(tar.memory_page)
		{
			auto last_index = tar.memory_page->available_count;
			tar.memory_page->available_count += 1;
			return last_index;
		}

		return std::nullopt;
	}

	void ArchetypeComponentManager::CopyMountPointFormLast(Element& tar, std::size_t mp_index)
	{
		assert(tar.memory_page);
		auto mp = tar.memory_page->GetMountPoint();
		assert(mp.available_count > mp_index);
		auto last_index = mp.available_count - 1;
		if(last_index != mp_index)
		{
			auto pro = static_cast<EntityProperty*>(tar.archetype->Get(tar.entity_property_locate, mp, last_index));
			auto tar_entity = pro->GetEntity();
			tar.archetype->MoveConstruct(mp, mp_index, mp, last_index);
			tar.archetype->Destruct(mp, last_index);
			std::lock_guard lg(tar_entity->mutex);
			tar_entity->mount_point_index = mp_index;
		}
		tar.memory_page->available_count -= 1;
		if(tar.memory_page->available_count == 0)
		{
			tar.memory_page.Reset();
		}
	}
}