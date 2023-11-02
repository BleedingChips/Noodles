module;

#include <cassert>

module NoodlesComponent;

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
				std::size_t last_size = min_page_size - sizeof(ComponentPage);
				if(std::align(component_layout.Align, last_size, buffer, last_size) != nullptr)
				{
					std::span<std::byte> real_buffer{ static_cast<std::byte*>(buffer), last_size };
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

	

	bool ArchetypeComponentManager::EntityConstructor::Construct(UniqueTypeID const& id, void* source, std::size_t i)
	{
		assert(archetype_ptr);
		auto index = archetype_ptr->LocateTypeID(id, i);
		if(index.has_value())
		{
			if(status[*index] == 0)
			{
				auto tar_buffer = archetype_ptr->GetData(*index, mount_point);
				assert(tar_buffer != nullptr);
				archetype_ptr->MoveConstruct(*index, tar_buffer, source);
				status[*index] = 1;
				return true;
			}
		}
		return false;
	}

	ArchetypeComponentManager::ArchetypeComponentManager(std::pmr::memory_resource* upstream)
		:components(upstream), resource(upstream), spawned_entities(upstream), spawned_entities_resource(upstream),
		archetype_resource(upstream), entity_resource(decltype(entity_resource)::Type::Create(upstream)),
		removed_entities(upstream)
	{

	}

	auto ArchetypeComponentManager::CreateEntityConstructor(std::span<ArchetypeID const> ids)
		->EntityConstructor
	{
		static std::array<ArchetypeID, 1> build_in = {
			ArchetypeID::Create<EntityProperty>()
		};

		std::lock_guard lg(spawned_entities_resource_mutex);
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
					std::move(ptr),
					mp
				};
			}
		}
		return {};
	}

	Entity ArchetypeComponentManager::CreateEntity(EntityConstructor&& constructor)
	{
		if(constructor.archetype_ptr && constructor.mount_point)
		{
			auto loc = constructor.archetype_ptr->LocateTypeID(
				UniqueTypeID::Create<EntityProperty>()
			);
			if(loc.has_value() && constructor.status[*loc] == 0)
			{
				std::size_t cur_i = 0;
				for(auto ite : constructor.status)
				{
					if(ite == 0 && cur_i != *loc)
					{
						return {};
					}
					++cur_i;
				}
				assert(entity_resource);
				Entity entity = EntityStorage::Create(entity_resource->get_resource_interface());
				if(entity)
				{
					EntityConstructor temp_construct {std::move(constructor)};
					entity->archetype = std::move(temp_construct.archetype_ptr);
					entity->mount_point = temp_construct.mount_point;
					auto buffer = entity->archetype->GetData(*loc, entity->mount_point);
					EntityProperty pro{
						entity
					};
					entity->archetype->MoveConstruct(*loc, buffer, &pro);
					std::lock_guard lg(spawn_mutex);
					spawned_entities.push_back(entity);
					return entity;
				}
			}
		}
		return {};
	}

}