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
			if (component_layout.align > alignof(ComponentPage))
			{
				fix_size = component_layout.align - alignof(ComponentPage);
			}
			std::size_t element_count = 0;
			while (true)
			{
				auto buffer_size = (min_page_size - fix_size - self_size);
				element_count = buffer_size / component_layout.size;
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
				std::size_t require_size = component_layout.align * 2;
				if(std::align(component_layout.align, component_layout.align, buffer, require_size) != nullptr)
				{
					auto start = static_cast<std::byte*>(buffer);
					assert(static_cast<std::byte*>(adress) + min_page_size >= start + component_layout.size * min_element_count);
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
				ite.atomic_type->Destruction(ite.record.Get());
				ite.record.Deallocate();
			}
		}
		datas.clear();
	}

	void Entity::SetFree()
	{
		status = EntityStatus::Free;
		archetype_index.Reset();
		mount_point_index.Reset();
		modify_index.Reset();
		AtomicTypeMark::Reset(current_component_mask);
		AtomicTypeMark::Reset(modify_component_mask);
	}

	auto Entity::Create(AtomicTypeManager const& manager, std::pmr::memory_resource* resource)
		-> Ptr
	{
		auto layout = Potato::MemLayout::MemLayoutCPP::Get<Entity>();
		auto offset_current_mask = layout.Insert(Potato::MemLayout::Layout::GetArray<AtomicTypeMark>(manager.GetStorageCount()));
		auto offset_modify_component = layout.Insert(Potato::MemLayout::Layout::GetArray<AtomicTypeMark>(manager.GetStorageCount()));

		auto record = Potato::IR::MemoryResourceRecord::Allocate(resource, layout.Get());
		if (record)
		{
			auto current_mask = std::span{
				new (record.GetByte(offset_current_mask)) AtomicTypeMark[manager.GetStorageCount()],
				manager.GetStorageCount()
			};
			auto modify_component = std::span{
				new (record.GetByte(offset_modify_component)) AtomicTypeMark[manager.GetStorageCount()],
				manager.GetStorageCount()
			};
			return new (record.Get()) Entity{
				record,
				current_mask,
				modify_component
			};
		}
		
		return {};
	}

	Entity::~Entity()
	{
		AtomicTypeMark::Destruction(current_component_mask);
		AtomicTypeMark::Destruction(modify_component_mask);
	}

	ComponentFilter::Ptr ComponentFilter::Create(
		AtomicTypeManager& manager,
		std::span<Info const> require_component_type,
		std::span<AtomicType::Ptr const> refuse_component_type,
		std::pmr::memory_resource* storage_resource,
		std::pmr::memory_resource* archetype_info_resource
	)
	{
		auto layout = Potato::MemLayout::MemLayoutCPP::Get<ComponentFilter>();
		auto require_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<AtomicTypeMark>(manager.GetStorageCount()));
		auto require_write_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<AtomicTypeMark>(manager.GetStorageCount()));
		auto refuse_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<AtomicTypeMark>(manager.GetStorageCount()));
		auto atomic_index_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<AtomicTypeID>(require_component_type.size()));
		auto re = Potato::IR::MemoryResourceRecord::Allocate(storage_resource, layout.Get());
		if(re)
		{
			std::span<AtomicTypeMark> require_mask { new (re.GetByte(require_offset)) AtomicTypeMark[manager.GetStorageCount()], manager.GetStorageCount() };
			std::span<AtomicTypeMark> require_write_mask{ new (re.GetByte(require_write_offset)) AtomicTypeMark[manager.GetStorageCount()], manager.GetStorageCount() };
			std::span<AtomicTypeID> require_index { new (re.GetByte(atomic_index_offset)) AtomicTypeID[require_component_type.size()], require_component_type.size() };

			auto ite_span = require_index;

			for(auto& Ite : require_component_type)
			{
				auto loc = manager.LocateOrAddAtomicType(Ite.atomic_type);
				if(loc.has_value())
				{
					AtomicTypeMark::Mark(require_mask, *loc);
					ite_span[0] = *loc;
					ite_span = ite_span.subspan(1);
					if(Ite.need_write)
					{
						AtomicTypeMark::Mark(require_write_mask, *loc);
					}
				}else
				{
					assert(false);
					AtomicTypeMark::Destruction(require_mask);
					AtomicTypeMark::Destruction(require_write_mask);
					AtomicTypeID::Destruction(require_index);
				}
			}

			std::span<AtomicTypeMark> refuse_mask{ new (re.GetByte(refuse_offset)) AtomicTypeMark[manager.GetStorageCount()], manager.GetStorageCount() };

			for(auto& Ite : refuse_component_type)
			{
				auto loc = manager.LocateOrAddAtomicType(Ite);
				if (loc.has_value())
				{
					AtomicTypeMark::Mark(refuse_mask, *loc);
				}
				else
				{
					assert(false);
					AtomicTypeMark::Destruction(require_mask);
					AtomicTypeMark::Destruction(require_write_mask);
					AtomicTypeID::Destruction(require_index);
					AtomicTypeMark::Destruction(refuse_mask);
				}
			}

			return new (re.Get()) ComponentFilter { re, require_mask, require_write_mask, refuse_mask, require_index, archetype_info_resource };
 		}
		return {};
	}

	bool ComponentFilter::OnCreatedArchetype(std::size_t archetype_index, Archetype const& archetype)
	{
		std::lock_guard lg(mutex);
		auto archetype_atomic_id = archetype.GetAtomicTypeMark();
		if(AtomicTypeMark::Inclusion(archetype_atomic_id, GetRequiredAtomicMarkArray()) && !AtomicTypeMark::IsOverlapping(archetype_atomic_id, GetRefuseAtomicMarkArray()))
		{
			auto old_size = archetype_offset.size();
			auto atomic_span = GetAtomicID();
			auto atomic_size = atomic_span.size();
			archetype_offset.resize(old_size + 1 + atomic_size);
			auto span = std::span(archetype_offset).subspan(old_size);
			span[0] = archetype_index;
			for(std::size_t i = 0; i < atomic_size; ++i)
			{
				auto index = archetype.LocateAtomicTypeID(atomic_span[i]);
				assert(index.has_value());
				span[i + 1] = archetype.GetMemberView()[index->index].offset;
			}
			return true;
		}
		return false;
	}

	std::optional<std::span<std::size_t const>> ComponentFilter::EnumMountPointIndexByArchetypeIndex_AssumedLocked(std::size_t archetype_index) const
	{
		auto size = GetAtomicID().size();
		auto span = std::span(archetype_offset);
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
		auto size = GetAtomicID().size();
		auto span = std::span(archetype_offset);
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

	SingletonFilter::Ptr SingletonFilter::Create(
		AtomicTypeManager& manager,
		std::span<Info const> require_singleton,
		std::pmr::memory_resource* storage_resource
	)
	{
		auto layout = Potato::MemLayout::MemLayoutCPP::Get<SingletonFilter>();
		auto require_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<AtomicTypeMark>(manager.GetStorageCount()));
		auto require_write_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<AtomicTypeMark>(manager.GetStorageCount()));
		auto atomic_index_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<AtomicTypeID>(require_singleton.size()));
		auto reference_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<std::size_t>(require_singleton.size()));
		auto re = Potato::IR::MemoryResourceRecord::Allocate(storage_resource, layout.Get());
		if (re)
		{
			std::span<AtomicTypeMark> require_mask{ new (re.GetByte(require_offset)) AtomicTypeMark[manager.GetStorageCount()], manager.GetStorageCount() };
			std::span<AtomicTypeMark> require_write_mask{ new (re.GetByte(require_write_offset)) AtomicTypeMark[manager.GetStorageCount()], manager.GetStorageCount() };
			std::span<AtomicTypeID> require_index{ new (re.GetByte(atomic_index_offset)) AtomicTypeID[require_singleton.size()], require_singleton.size() };
			std::span<std::size_t> reference_span{ new (re.GetByte(reference_offset)) std::size_t[require_singleton.size()], require_singleton.size() };

			for(auto& ite : reference_span)
			{
				ite = std::numeric_limits<std::size_t>::max();
			}

			auto ite_span = require_index;

			for (auto& Ite : require_singleton)
			{
				auto loc = manager.LocateOrAddAtomicType(Ite.atomic_type);
				if (loc.has_value())
				{
					AtomicTypeMark::Mark(require_mask, *loc);
					ite_span[0] = *loc;
					ite_span = ite_span.subspan(1);
					if (Ite.need_write)
					{
						AtomicTypeMark::Mark(require_write_mask, *loc);
					}
				}
				else
				{
					assert(false);
					AtomicTypeMark::Destruction(require_mask);
					AtomicTypeMark::Destruction(require_write_mask);
					AtomicTypeID::Destruction(require_index);
				}
			}

			return new (re.Get()) SingletonFilter{ re, require_mask, require_write_mask, require_index, reference_span };
		}
		return {};
	}

	bool SingletonFilter::OnSingletonModify(Archetype const& archetype)
	{
		std::lock_guard lg(mutex);
		auto span = GetAtomicID();
		auto ref = archetype_offset;
		for(auto& ite : span)
		{
			auto loc = archetype.LocateAtomicTypeID(ite);
			if(loc.has_value())
			{
				ref[0] = archetype[loc->index].offset;
			}else
			{
				ref[0] = std::numeric_limits<std::size_t>::max();
			}
			ref = ref.subspan(1);
		}
		return true;
	}

	/*
	std::optional<std::span<void*>> ArchetypeComponentManager::ReadEntityDirect_AssumedLocked(Entity const& entity, ComponentFilter const& filter, std::span<void*> output_ptr, bool prefer_modify) const
	{
		std::shared_lock lg(entity.mutex);
		std::shared_lock lg2(filter.mutex);
		
		prefer_modify = (entity.modify_index != std::numeric_limits<std::size_t>::max()) && prefer_modify;
		auto offset = filter.EnumMountPointIndexByArchetypeIndex_AssumedLocked(entity.archetype_index);
		if(offset.has_value() && output_ptr.size() >= offset->size())
		{
			auto [mp, index] = GetComponentPage(entity.archetype_index);
			for (auto& ite : span)
			{
				bool find = false;
				void* data = nullptr;
				if (prefer_modify)
				{
					for (auto& ite2 : entity.modifer->datas)
					{
						if (ite2.available && *ite2.atomic_type == *ite)
						{
							find = true;
							if (ite2.record)
							{
								data = ite2.record.Get();
							}
							break;
						}
					}
				}
				if (data == nullptr && find)
				{
					if (mp)
					{
						auto loc = mp->LocateTypeID(*ite);
						if (loc)
						{
							data = mp->Get(*loc, index, entity.mount_point_index);
						}
					}
				}
				output_ptr[span_index++] = data;
			}
		}


		if(entity.owner_id == reinterpret_cast<std::size_t>(this))
		{
			auto span = filter.GetAtomicType();
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
							if(ite2.available && *ite2.atomic_type == *ite)
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
							auto loc = mp->LocateTypeID(*ite);
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
	*/

	auto ArchetypeComponentManager::ReadComponents_AssumedLocked(ComponentFilter const& filter, std::size_t filter_ite, std::pmr::memory_resource* wrapper_resource) const
		->std::optional<DataWrapper>
	{
		std::shared_lock sl(filter.mutex);
		std::size_t archetype_index = 0;
		auto re = filter.EnumMountPointIndexByIterator_AssumedLocked(filter_ite, archetype_index);
		if(re.has_value())
		{
			assert(components.size() > archetype_index);
			auto& ref = components[archetype_index];
			Archetype::OPtr archetype = ref.archetype;
			auto page = ref.memory_page;
			if(page)
			{
				std::pmr::vector<void*> temp_elements{ wrapper_resource };
				temp_elements.resize(re->size());

				for(std::size_t i = 0; i < temp_elements.size(); ++i)
				{
					temp_elements[i] = static_cast<std::byte*>(page->GetMountPoint().GetBuffer()) + (*re)[i];
				}

				return DataWrapper
				{
					std::move(archetype),
					page->available_count,
					std::move(temp_elements)
				};
			}else
			{
				DataWrapper wrapper
				{
					std::move(archetype),
					0,
					{}
				};
				return wrapper;
			}
		}
		return std::nullopt;
	}

	auto ArchetypeComponentManager::ReadEntityComponents_AssumedLocked(Entity const& ent, ComponentFilter const& filter, std::pmr::memory_resource* wrapper_resource) const
		->std::optional<DataWrapper>
	{
		std::shared_lock sl(ent.mutex);
		if(ent.status == EntityStatus::Normal)
		{
			std::shared_lock sl(filter.mutex);
			std::size_t archetype_index = 0;
			auto re = filter.EnumMountPointIndexByArchetypeIndex_AssumedLocked(ent.archetype_index);
			if (re.has_value())
			{
				assert(components.size() > archetype_index);
				auto& ref = components[archetype_index];
				Archetype::OPtr archetype = ref.archetype;
				auto page = ref.memory_page;
				if (page)
				{
					std::pmr::vector<void*> temp_elements{ wrapper_resource };
					temp_elements.resize(re->size());

					for (std::size_t i = 0; i < temp_elements.size(); ++i)
					{
						temp_elements[i] = static_cast<std::byte*>(page->GetMountPoint().GetBuffer()) + (*re)[i];
					}

					return DataWrapper
					{
						std::move(archetype),
						page->available_count,
						std::move(temp_elements)
					};
				}
				else
				{
					DataWrapper wrapper
					{
						std::move(archetype),
						0,
						{}
					};
					return wrapper;
				}
			}
			return std::nullopt;
		}
		
	}

	auto ArchetypeComponentManager::ReadSingleton_AssumedLocked(SingletonFilter const& filter, std::pmr::memory_resource* wrapper_resource) const
		-> DataWrapper
	{
		std::shared_lock sl(filter.mutex);
		auto type_id = filter.GetAtomicID();
		auto span = filter.EnumSingleton_AssumedLocked();
		Archetype::OPtr archetype = singleton_archetype;
		std::pmr::vector<void*> elements{ wrapper_resource };
		if(archetype)
		{
			elements.resize(span.size());
			for (std::size_t i = 0; i < span.size(); ++i)
			{
				auto& ref = elements[i];
				if (span[i] == std::numeric_limits<std::size_t>::max())
				{
					elements[i] = nullptr;
				}
				else
				{
					elements[i] = singleton_record.GetByte(span[i]);
				}
			}
			return DataWrapper
			{
				std::move(archetype),
				1,
				std::move(elements)
			};
		}
		return DataWrapper{};
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

	bool ArchetypeComponentManager::MoveAndCreateSingleton(AtomicType::Ptr atomic_type, void* move_reference, std::pmr::memory_resource* wrapper_resource)
	{
		std::lock_guard lg(singleton_modifier_mutex);
		auto loc = singleton_manager.LocateOrAddAtomicType(atomic_type);
		if (loc.has_value())
		{
			auto record = Potato::IR::MemoryResourceRecord::Allocate(wrapper_resource, atomic_type->GetLayout());
			if(record)
			{
				auto re = atomic_type->MoveConstruction(record.Get(), move_reference);
				assert(re);
				singleton_modifier.emplace_back(
					*loc,
					std::move(atomic_type),
					record
				);
				return true;
			}
		}
		return false;
	}

	ComponentFilter::Ptr ArchetypeComponentManager::CreateComponentFilter(
		std::span<ComponentFilter::Info const> require_component,
		std::span<AtomicType::Ptr const> refuse_component,
		std::size_t identity,
		std::pmr::memory_resource* filter_resource,
		std::pmr::memory_resource* offset_resource
	)
	{
		auto filter = ComponentFilter::Create(
			component_manager,
			require_component,
			refuse_component,
			filter_resource,
			offset_resource
		);
		if(filter)
		{
			{
				std::shared_lock sl(component_mutex);
				for(std::size_t i = 0; i < components.size(); ++i)
				{
					filter->OnCreatedArchetype(i, *components[i].archetype);
				}
			}

			std::lock_guard lg(filter_mutex);
			component_filter.emplace_back(
				std::move(filter),
				OptionalIndex{identity}
			);
			return filter;
		}
		return {};
	}

	SingletonFilter::Ptr ArchetypeComponentManager::CreateSingletonFilter(
		std::span<ComponentFilter::Info const> require_singleton,
		std::size_t identity,
		std::pmr::memory_resource* filter_resource
	)
	{
		auto filter = SingletonFilter::Create(
			component_manager,
			require_singleton,
			filter_resource
		);
		if (filter)
		{
			{
				std::shared_lock sl(component_mutex);
				if(singleton_archetype)
					filter->OnSingletonModify(*singleton_archetype);
			}

			std::lock_guard lg(filter_mutex);
			singleton_filters.emplace_back(
				std::move(filter),
				OptionalIndex{ identity }
			);
			return filter;
		}
		return {};
	}

	ArchetypeComponentManager::ArchetypeComponentManager(Setting setting, std::pmr::memory_resource* resource)
		:
		component_manager(setting.max_component_atomic_type_count, resource),
		singleton_manager(setting.max_singleton_atomic_type_count, resource),
		components(resource),
		modified_entity(resource),
		singleton_modifier(resource),
		component_filter(resource),
		singleton_filters(resource)
	{
		auto loc = component_manager.LocateOrAddAtomicType(GetAtomicType<EntityProperty>());
		assert(loc && loc->index == 0);
	}

	ArchetypeComponentManager::EntityModifierEvent::~EntityModifierEvent()
	{
		if(resource)
		{
			if(atomic_type)
			{
				auto re = atomic_type->Destruction(resource.Get());
				assert(re);
				resource.Deallocate();
			}
		}
	}

	ArchetypeComponentManager::EntityModifierEvent::EntityModifierEvent(EntityModifierEvent&& event)
		: operation(event.operation), index(event.index), atomic_type(std::move(event.atomic_type)), resource(event.resource)
	{
		event.resource = {};
	}

	ArchetypeComponentManager::SingletonModifier::~SingletonModifier()
	{
		if (resource)
		{
			if (atomic_type)
			{
				auto re = atomic_type->Destruction(resource.Get());
				assert(re);
				resource.Deallocate();
			}
		}
	}

	
	ArchetypeComponentManager::~ArchetypeComponentManager()
	{

		{
			std::lock_guard lg(singleton_modifier_mutex);
			singleton_modifier.clear();
		}

		{
			std::lock_guard lg(entity_modifier_mutex);
			modified_entity.clear();
		}

		{
			std::lock_guard lg2(component_mutex);
			if(singleton_record && singleton_archetype)
			{
				for(auto& ite : singleton_archetype->GetMemberView())
				{
					ite.layout->Destruction(
						singleton_record.GetByte(ite.offset)
					);
				}
				singleton_record.Deallocate();
			}
			for(auto& ite : components)
			{
				auto ite2 = ite.memory_page;
				if(ite.memory_page)
				{
					auto mp = ite.memory_page->GetMountPoint();
					auto index = ite.memory_page->available_count;
					auto entity_property = ite.archetype->Get(ite.entity_property_locate.index, mp).Translate<EntityProperty>();
					for(auto& ite : entity_property)
					{
						auto entity = ite.GetEntity();
						assert(entity);
						std::lock_guard lg(entity->mutex);
						entity->SetFree();
					}
					auto mv = ite.archetype->GetMemberView();
					for (auto& ite3 : mv)
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



	EntityPtr ArchetypeComponentManager::CreateEntity(std::pmr::memory_resource* entity_resource, std::pmr::memory_resource* temp_resource)
	{
		auto ent = Entity::Create(component_manager, entity_resource);
		if(ent)
		{
			EntityProperty entity_property
			{
				ent
			};

			auto re = AddEntityComponent_AssumedLocked(*ent, *GetAtomicType<EntityProperty>(), &entity_property, true, temp_resource);
			assert(re);
			return ent;
		}
		return {};
	}

	bool ArchetypeComponentManager::ReleaseEntity(Entity& ptr, std::pmr::memory_resource* temp_resource)
	{
		{
			std::lock_guard lg(ptr.mutex);
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
			std::lock_guard lg2(entity_modifier_mutex);
			if(ptr.modify_index)
			{
				auto& ref = modified_entity[ptr.modify_index];
				ref.need_remove = true;
				ref.modifier_event.clear();
			}else
			{
				ptr.modify_index = modified_entity.size();
				std::pmr::vector<EntityModifierEvent> temp_modifier{ temp_resource };
				EntityModifier modifer
				{
					&ptr,
					true,
					std::move(temp_modifier)
				};
				modified_entity.emplace_back(
					std::move(modifer)
				);
			}
			return true;
		}
		return true;
	} 

	bool ArchetypeComponentManager::AddEntityComponent_AssumedLocked(Entity& target_entity, AtomicType const& atomic_type, void* reference_buffer, bool accept_build_in_component, std::pmr::memory_resource* resource)
	{
		auto ope = atomic_type.GetOperateProperty();
		if(!ope.move_construct && !ope.copy_construct)
			return false;
		auto loc = component_manager.LocateOrAddAtomicType(&atomic_type);
		if(loc.has_value() && (accept_build_in_component || *loc != GetEntityPropertyAtomicTypeID()))
		{
			std::lock_guard lg(target_entity.mutex);
			if(target_entity.status == EntityStatus::Normal || target_entity.status == EntityStatus::PreInit)
			{
				auto re = AtomicTypeMark::CheckIsMark(target_entity.modify_component_mask, *loc);
				if(re && !re)
				{
					auto record = Potato::IR::MemoryResourceRecord::Allocate(resource, atomic_type.GetLayout());
					if(record)
					{
						if (!atomic_type.MoveConstruction(record.Get(), reference_buffer))
						{
							auto re = atomic_type.CopyConstruction(record.Get(), reference_buffer);
							assert(re);
						}

						if(!target_entity.modify_index)
						{
							target_entity.modify_index = modified_entity.size();
							std::pmr::vector<EntityModifierEvent> temp{ resource };
							temp.emplace_back(
								EntityModifierEvent::Operation::AddComponent,
								*loc,
								&atomic_type,
								record
							);
							modified_entity.emplace_back(
								&target_entity,
								false,
								std::move(temp)
							);
						}else
						{
							auto& ref = modified_entity[target_entity.modify_index];
							ref.modifier_event.emplace_back(
								EntityModifierEvent::Operation::AddComponent,
								*loc,
								&atomic_type,
								record
							);
						}

						auto re = AtomicTypeMark::Mark(target_entity.modify_component_mask, *loc);
						assert(re);
						return true;
					}
				}
			}
		}
		return false;
	}

	bool ArchetypeComponentManager::RemoveEntityComponent(Entity& target_entity, AtomicType const& atomic_type, bool accept_build_in_component, std::pmr::memory_resource* resource)
	{
		auto loc = component_manager.LocateOrAddAtomicType(&atomic_type);
		if (loc.has_value() && (accept_build_in_component || *loc != GetEntityPropertyAtomicTypeID()))
		{
			std::lock_guard lg(target_entity.mutex);
			if(target_entity.status == EntityStatus::Normal || target_entity.status == EntityStatus::PreInit)
			{
				auto marked = AtomicTypeMark::CheckIsMark(target_entity.modify_component_mask, *loc);
				if (marked && *marked)
				{
					if(!target_entity.modify_index)
					{
						target_entity.modify_index = modified_entity.size();
						std::pmr::vector<EntityModifierEvent> temp{resource};
						temp.emplace_back(
							EntityModifierEvent::Operation::RemoveComponent,
							*loc,
							&atomic_type,
							Potato::IR::MemoryResourceRecord{}
						);
						modified_entity.emplace_back(
							&target_entity,
							false,
							std::move(temp)
						);
					}else
					{
						auto& ref = modified_entity[target_entity.modify_index];
						for(auto ite = ref.modifier_event.rbegin(); ite != ref.modifier_event.rend(); ++ite)
						{
							if(ite->index == *loc)
							{
								ite->~EntityModifierEvent();
								new (&(*ite))  EntityModifierEvent{
									EntityModifierEvent::Operation::Ignore,
									*loc, {}, {}
								};
								auto re = AtomicTypeMark::Mark(target_entity.modify_component_mask, *loc, false);
								assert(re);
								return true;
							}
						}
						ref.modifier_event.emplace_back(
							EntityModifierEvent::Operation::RemoveComponent,
							*loc, AtomicType::Ptr{}, Potato::IR::MemoryResourceRecord{}
						);
						auto re = AtomicTypeMark::Mark(target_entity.modify_component_mask, *loc, false);
						assert(re);
						return true;
					}
				}
			}
		}
		return false;
	}

	bool ArchetypeComponentManager::ForceUpdateState(
		std::pmr::memory_resource* archetype_resource,
		std::pmr::memory_resource* temp_resource
		)
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
			
			for(auto& ite : modified_entity)
			{
				if(ite.reference_entity && ite.need_remove)
				{
					auto& entity = *ite.reference_entity;
					std::lock_guard lg(entity.mutex);
					assert(entity.status == EntityStatus::PendingDestroy);
					entity.status = EntityStatus::Free;
					if(entity.archetype_index)
					{
						auto& ref = components[entity.archetype_index];
						auto mount_point = ref.memory_page->GetMountPoint();
						ref.archetype->Destruct(mount_point, entity.mount_point_index);

						remove_list.emplace_back(
							entity.archetype_index,
							entity.mount_point_index
						);

					}
					entity.SetFree();
					ite.reference_entity.Reset();
				}
			}

			struct AtomicElement
			{
				AtomicTypeID index;
				Potato::IR::Layout layout;
				AtomicType::Ptr atomic_type;
			};

			std::pmr::vector<AtomicType::Ptr> reference_atomic{ temp_resource };
			std::pmr::vector<AtomicTypeMark> marks{ temp_resource };
			marks.resize(component_manager.GetStorageCount());

			for(auto& ite : modified_entity)
			{
				if(ite.reference_entity)
				{
					auto& entity = *ite.reference_entity;
					std::lock_guard lg(entity.mutex);
					if(AtomicTypeMark::IsSame(entity.modify_component_mask, entity.current_component_mask))
					{
						reference_atomic.clear();
					
						auto& element = components[entity.archetype_index];
						for(auto& ite2 : ite.modifier_event)
						{
							if(ite2.operation == EntityModifierEvent::Operation::AddComponent)
							{
								auto index = element.archetype->LocateAtomicTypeID(ite2.index);
								assert(index);
								auto& mm = element.archetype->GetMemberView(*index);
								element.archetype->Destruct(
									mm,
									element.memory_page->GetMountPoint(),
									entity.mount_point_index
								);
								element.archetype->MoveConstruct(
									mm,
									element.archetype->Get(mm, element.memory_page->GetMountPoint(), entity.mount_point_index),
									ite2.resource.Get()
								);
							}
						}
					}else
					{
						OptionalIndex new_archetype_index;
						std::size_t new_mount_point_index = 0;
						for (std::size_t i = 0; i < components.size(); ++i)
						{
							if (AtomicTypeMark::IsSame(entity.modify_component_mask, components[i].archetype->GetAtomicTypeMark()))
							{
								new_archetype_index = i;
								break;
							}
						}

						if(!new_archetype_index)
						{
							new_archetype_index = components.size();
							reference_atomic.clear();
							AtomicTypeMark::Reset(marks);

							for (auto& ite2 : ite.modifier_event)
							{
								if(ite2.operation == EntityModifierEvent::Operation::AddComponent)
								{
									reference_atomic.emplace_back(
										ite2.atomic_type
									);
									auto re = AtomicTypeMark::Mark(marks, ite2.index);
									assert(re && !*re);
								}
							}

							auto& element = components[entity.archetype_index];

							auto mm = element.archetype->GetMemberView();

							for(auto& ite2 : mm)
							{
								auto re = AtomicTypeMark::CheckIsMark(entity.modify_component_mask, ite2.atomic_type_id);
								
								if(re && *re)
								{
									re = AtomicTypeMark::CheckIsMark(marks, ite2.atomic_type_id);
									if(re && !re)
									{
										reference_atomic.emplace_back(
											ite2.layout
										);
										re = AtomicTypeMark::Mark(marks, ite2.atomic_type_id);
										assert(re && !*re);
									}
								}
							}

							std::sort(
								reference_atomic.begin(), reference_atomic.end(),
								[](AtomicType::Ptr const& i1, AtomicType::Ptr const& i2)
								{
									auto order = Potato::Misc::PriorityCompareStrongOrdering(
										i1->GetLayout().align, i2->GetLayout().align,
										i1->GetLayout().size, i2->GetLayout().size
									);
									if(order == std::strong_ordering::greater)
									{
										return true;
									}
									return false;
								}
							);

							auto ptr = Archetype::Create(
								component_manager, reference_atomic, archetype_resource
							);
							assert(ptr);
							components.emplace_back(
								std::move(ptr),
								ComponentPage::Ptr{},
								*ptr->LocateAtomicTypeID(GetEntityPropertyAtomicTypeID()),
								0
							);
							new_mount_point_index = *AllocateMountPoint(*components.rbegin());
						}else
						{
							auto find = std::find_if(
								remove_list.begin(), remove_list.end(),
								[=](RemoveEntity i1)
								{
									return i1.archetype_index == new_archetype_index.Get();
								}
							);
							if(find == remove_list.end())
							{
								new_mount_point_index = *AllocateMountPoint(components[new_archetype_index]);
							}else
							{
								new_mount_point_index = find->mount_point_index;
								remove_list.erase(find);
							}
						}

						assert(new_archetype_index);

						AtomicTypeMark::Reset(marks);

						auto& old_ref = components[entity.archetype_index];
						auto old_mount_point = old_ref.memory_page->GetMountPoint();
						auto& ref = components[new_archetype_index];
						auto new_mount_point = ref.memory_page->GetMountPoint();

						for(auto& ite2 : ite.modifier_event)
						{
							if(ite2.operation == EntityModifierEvent::Operation::AddComponent)
							{
								auto index = ref.archetype->LocateAtomicTypeID(ite2.index);
								assert(index);
								auto& mm = ref.archetype->GetMemberView(*index);
								ref.archetype->MoveConstruct(
									mm,
									ref.archetype->Get(mm, new_mount_point, new_mount_point_index),
									ite2.resource.Get()
								);
								auto re = AtomicTypeMark::Mark(marks, ite2.index);
								assert(re && !*re);
							}
						}

						auto mm = old_ref.archetype->GetMemberView();

						for (auto& ite2 : mm)
						{
							auto re = AtomicTypeMark::CheckIsMark(entity.modify_component_mask, ite2.atomic_type_id);

							if (re && *re)
							{
								re = AtomicTypeMark::CheckIsMark(marks, ite2.atomic_type_id);
								if (re && !re)
								{
									auto index = ref.archetype->LocateAtomicTypeID(ite2.atomic_type_id);
									assert(index);
									auto& mm = ref.archetype->GetMemberView(*index);

									ref.archetype->MoveConstruct(
										mm,
										ref.archetype->Get(mm, new_mount_point, new_mount_point_index),
										old_ref.archetype->Get(ite2, old_mount_point, entity.mount_point_index)
									);
									re = AtomicTypeMark::Mark(marks, ite2.atomic_type_id);
									assert(re && !*re);
								}
							}
						}

						old_ref.archetype->Destruct(
							old_mount_point,
							entity.mount_point_index
						);

						remove_list.emplace_back(
							entity.archetype_index,
							entity.mount_point_index
						);

						entity.status = EntityStatus::Normal;
						entity.modify_index.Reset();
						entity.mount_point_index = new_mount_point_index;
						entity.archetype_index = new_archetype_index;
					}
				}
			}

			modified_entity.clear();


			for (auto& ite : remove_list)
			{
				auto& ref = components[ite.archetype_index];
				CopyMountPointFormLast(ref, ite.mount_point_index);
				Updated = true;
			}

			remove_list.clear();

			{
				std::lock_guard lg2(filter_mutex);

				if (component_filter_need_remove)
				{
					component_filter_need_remove = false;
					component_filter.erase(
						std::remove_if(component_filter.begin(), component_filter.end(), [&](auto& ite)->bool
							{
								return !std::get<1>(ite);
							}),
						component_filter.end()
					);
				}

				if (old_component_size != components.size())
				{
					for(auto i = old_component_size; i < components.size(); ++i)
					{
						for(auto& ite : component_filter)
						{
							std::get<0>(ite)->OnCreatedArchetype(i, *components[i].archetype);
						}
					}
				}

				if (singleton_filter_need_remove)
				{
					singleton_filter_need_remove = false;
					singleton_filters.erase(
						std::remove_if(singleton_filters.begin(), singleton_filters.end(), [&](auto& ite)->bool
							{
								return !std::get<1>(ite);
							}),
						singleton_filters.end()
					);
				}
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