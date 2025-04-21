module;

#include <cassert>

module NoodlesEntity;
import NoodlesArchetype;

namespace Noodles
{
	void Entity::SetFree_AssumedLocked()
	{
		state = State::Free;
		index.reset();
		modify_component_bitflag.Reset();
		component_bitflag.Reset();
	}

	auto Entity::Create(std::size_t component_bitflag_container_count, std::pmr::memory_resource* resource)
		-> Ptr
	{
		auto layout = Potato::MemLayout::MemLayoutCPP::Get<Entity>();

		auto offset_flag = layout.Insert(Potato::MemLayout::Layout::GetArray<BitFlagContainerViewer::Element>(component_bitflag_container_count * 2));
		auto record = Potato::IR::MemoryResourceRecord::Allocate(resource, layout.Get());

		if (record)
		{
			auto flag_span = std::span{
				new (record.GetByte(offset_flag)) BitFlagContainerViewer::Element[component_bitflag_container_count * 2],
				component_bitflag_container_count * 2
			};

			for (auto& ite : flag_span)
			{
				ite = 0;
			}

			return new (record.Get()) Entity{
				record,
				BitFlagContainerViewer{flag_span.subspan(0, component_bitflag_container_count)},
				BitFlagContainerViewer{flag_span.subspan(component_bitflag_container_count)},
			};
		}

		return {};
	}

	EntityProperty::~EntityProperty()
	{
		if (entity)
		{
			std::lock_guard lg(entity->mutex);
			entity->SetFree_AssumedLocked();
		}
	}

	EntityManager::EntityManager(AsynClassBitFlagMap& map, Config config)
		: entity_modifier(config.resource), entity_modifier_event(config.resource)
	{
		auto result = map.LocateOrAdd(*StructLayout::GetStatic<EntityProperty>());
		assert(result.has_value());
		entity_property_bitflag = *result;
		componenot_bitflag_container_count = map.GetBitFlagContainerElementCount();
	}

	EntityManager::~EntityManager()
	{
		for (auto& ite : entity_modifier_event)
		{
			ite.Release();
		}
		entity_modifier_event.clear();
		entity_modifier.clear();
	}

	Entity::Ptr EntityManager::CreateEntity(std::pmr::memory_resource* entity_resource, std::pmr::memory_resource* temp_resource)
	{
		auto ent = Entity::Create(componenot_bitflag_container_count, entity_resource);
		if (ent)
		{
			EntityProperty entity_property
			{
				ent
			};

			auto re = AddEntityComponentImp(*ent, *StructLayout::GetStatic<EntityProperty>(), &entity_property, GetEntityPropertyBitFlag(), true, Operation::Move, temp_resource);
			assert(re);
			return ent;
		}
		return {};
	}

	bool EntityManager::EntityModifierEvent::Release()
	{
		if (resource.Get() != nullptr)
		{
			assert(struct_layout && need_add);
			struct_layout->Destruction(resource.Get());
			struct_layout.Reset();
			resource = {};
			need_add = false;
			return true;
		}
		return false;
	}

	bool EntityManager::AddEntityComponentImp(Entity& entity, StructLayout const& component_class, void* component_ptr, BitFlag component_bitflag, bool accept_build_in, Operation operation, std::pmr::memory_resource* resource)
	{
		auto ope = component_class.GetOperateProperty();
		if (!ope.move_construct && !ope.copy_construct)
			return false;
		if (accept_build_in || component_bitflag != GetEntityPropertyBitFlag())
		{
			std::lock_guard lg(entity.mutex);
			if (entity.state == Entity::State::Normal || entity.state == Entity::State::PreInit)
			{
				auto re = entity.modify_component_bitflag.SetValue(component_bitflag);
				assert(re);
				if (!*re)
				{
					auto record = Potato::IR::MemoryResourceRecord::Allocate(resource, component_class.GetLayout());
					if (record)
					{
						if (operation == Operation::Copy)
						{
							auto re = component_class.CopyConstruction(record.Get(), component_ptr);
							assert(re);
						}
						else
						{
							auto re = component_class.MoveConstruction(record.Get(), component_ptr);
							assert(re);
						}

						decltype(entity_modifier_event)::iterator insert_ite = entity_modifier_event.end();

						if (!entity.modify_index)
						{
							entity.modify_index = entity_modifier.size();
							entity_modifier.emplace_back(
								false,
								Potato::Misc::IndexSpan<>{
								entity_modifier_event.size(),
									entity_modifier_event.size() + 1
							},
								&entity
							);
						}
						else
						{
							auto& ref = entity_modifier[entity.modify_index];
							insert_ite = ref.infos.End() + entity_modifier_event.begin();
							ref.infos.BackwardEnd(1);
						}

						entity_modifier_event.insert(
							insert_ite,
							EntityModifierEvent{
								true,
								component_bitflag,
								& component_class,
								record
							}
						);

						for (auto i = entity.modify_index.Get() + 1; i < entity_modifier.size(); ++i)
						{
							entity_modifier[i].infos.WholeOffset(1);
						}
						return true;
					}
				}
			}
		}
		return false;
	}

	bool EntityManager::ReleaseEntity(Entity& target_entity)
	{
		std::lock_guard lg(target_entity.mutex);
		if (target_entity.state == Entity::State::Normal || target_entity.state == Entity::State::PreInit)
		{
			target_entity.state = Entity::State::PendingDestroy;
			if (!target_entity.modify_index)
			{
				target_entity.modify_index = entity_modifier.size();
				entity_modifier.emplace_back(
					true,
					Potato::Misc::IndexSpan<>{
						entity_modifier_event.size(),
						entity_modifier_event.size()
					},
					&target_entity
				);
			}
			else
			{
				auto& ref = entity_modifier[target_entity.modify_index];
				ref.need_remove = true;
			}
			return true;
		}
		return false;
	}

	bool EntityManager::RemoveEntityComponentImp(Entity& target_entity, BitFlag const component_bitflag, bool accept_build_in)
	{
		if (accept_build_in || component_bitflag != GetEntityPropertyBitFlag())
		{
			std::lock_guard lg(target_entity.mutex);
			if (target_entity.state == Entity::State::Normal || target_entity.state == Entity::State::PreInit)
			{
				auto re = target_entity.modify_component_bitflag.SetValue(component_bitflag, false);
				assert(re.has_value());
				if (*re)
				{
					Potato::Misc::IndexSpan<> event_span;

					if (!target_entity.modify_index)
					{
						target_entity.modify_index = entity_modifier.size();
						entity_modifier.emplace_back(
							false,
							Potato::Misc::IndexSpan<>{
							entity_modifier_event.size(),
								entity_modifier_event.size()
						},
							& target_entity
						);
						event_span = { entity_modifier_event.size(), entity_modifier_event.size() };
					}
					else
					{
						event_span = entity_modifier[target_entity.modify_index].infos;
					}
					auto span = event_span.Slice(std::span(entity_modifier_event));
					for (auto& ite : span)
					{
						if (ite.need_add && ite.bitflag == component_bitflag)
						{
							ite.need_add = false;
							break;
						}
					}
					return true;
				}
			}
		}
		return false;
	}

	bool EntityManager::FlushEntityModify(ComponentManager& manager, std::pmr::memory_resource* temp_resource)
	{
		bool has_been_update = false;

		std::pmr::vector<ComponentManager::Index> removed_list(temp_resource);

		for (auto& ite : entity_modifier)
		{
			if (ite.entity && ite.need_remove)
			{
				has_been_update = true;
				auto& entity = *ite.entity;

				std::optional<Index> target_entity_index;
				{
					std::lock_guard lg(entity.mutex);
					assert(entity.state == Entity::State::PendingDestroy);
					target_entity_index = entity.index;
				}
				if (target_entity_index)
				{
					manager.DestructionEntity(
						*target_entity_index
					);
					removed_list.emplace_back(*target_entity_index);
				}
			}
		}

		std::pmr::vector<ComponentManager::Init> component_init_list{ temp_resource };
		std::pmr::vector<Archetype::Init> init_list{ temp_resource };

		for (auto& ite : entity_modifier)
		{
			if (ite.entity && !ite.need_remove)
			{
				has_been_update = true;
				auto& entity = *ite.entity;
				std::lock_guard lg(entity.mutex);
				auto bitflag_is_same = entity.modify_component_bitflag.IsSame(entity.component_bitflag);
				assert(bitflag_is_same.has_value());
				if (*bitflag_is_same)
				{
					assert(entity.index);
					auto span = ite.infos.Slice(std::span{ entity_modifier_event });
					component_init_list.clear();
					for (EntityModifierEvent const& ite2 : span)
					{
						if (ite2.need_add)
						{
							component_init_list.emplace_back(ite2.bitflag, true, ite2.resource.GetByte());
						}
					}
					ComponentManager::ConstructOption option;
					option.destruct_before_construct = true;
					auto re = manager.ConstructEntity(*entity.index, std::span(component_init_list), option);
					assert(re);

				}
				else {

					auto archetype_component_count = entity.modify_component_bitflag.GetBitFlagCount();
					auto archetype_index = manager.LocateComponentChunk(entity.modify_component_bitflag);

					init_list.clear();
					component_init_list.resize(archetype_component_count);
					std::size_t offset = 0;
					if (!archetype_index)
					{
						init_list.resize(archetype_component_count);
						offset = manager.FlushInitWithComponent(*entity.index, entity.modify_component_bitflag, std::span(component_init_list), std::span(init_list));
					}
					else {
						offset = manager.FlushInitWithComponent(*entity.index, entity.modify_component_bitflag, std::span(component_init_list), {});
					}

					auto span = ite.infos.Slice(std::span{ entity_modifier_event });
					std::size_t new_component_start_index = offset;
					for (EntityModifierEvent const& ite2 : span)
					{
						if (ite2.need_add)
						{
							bool modify = false;
							for (std::size_t i = 0; i < offset; ++i)
							{
								auto& cur = component_init_list[i];
								if (cur.component_class == ite2.bitflag)
								{
									cur.data = ite2.resource.GetByte();
									modify = true;
									break;
								}
							}
							if (!modify)
							{
								auto& cur = component_init_list[new_component_start_index];
								cur.component_class = ite2.bitflag;
								cur.data = ite2.resource.GetByte();
								if (!archetype_index)
								{
									auto& cur2 = init_list[new_component_start_index];
									cur2.flag = ite2.bitflag;
									cur2.ptr = ite2.struct_layout;
								}
								++new_component_start_index;
							}
						}
					}

					for (auto& ite : component_init_list)
					{
						ite.move_construct = true;
					}

					ComponentManager::ConstructOption option;
					option.destruct_before_construct = false;

					if (!archetype_index)
					{
						ComponentManager::Sort(init_list);
						archetype_index = manager.CreateComponentChunk(std::span(init_list));
						assert(archetype_index);
						if (!archetype_index)
						{
							continue;
						}
					}

					std::optional<Index> new_index;

					{
						auto find = std::find_if(removed_list.begin(), removed_list.end(), [=](ComponentManager::Index const& index) {
							return index.archetype_index == archetype_index.Get();
						});
						if (find != removed_list.end())
						{
							new_index = *find;
							removed_list.erase(find);
						}
					}

					if (!new_index)
					{
						new_index = manager.AllocateEntityWithoutConstruct(archetype_index);
						if (!new_index)
						{
							continue;
						}
					}

					auto re = manager.ConstructEntity(*new_index, std::span(component_init_list), option);
					assert(re);

					if (entity.index)
					{
						re = manager.DestructionEntity(*entity.index);
						assert(re);
						removed_list.emplace_back(*entity.index);
					}
					
					entity.index = new_index;
					entity.component_bitflag.CopyFrom(entity.modify_component_bitflag);
					entity.state = Entity::State::Normal;
					entity.modify_index.Reset();
				}
			}
		}

		for (auto& ite : entity_modifier_event)
		{
			ite.Release();
		}

		if (!removed_list.empty())
		{
			std::sort(removed_list.begin(), removed_list.end());
			std::span<ComponentManager::Index> span = std::span(removed_list);
			while (!span.empty())
			{
				std::size_t index = 1;
				for (std::size_t index = 1; index < span.size(); ++index)
				{
					if (span[index].archetype_index != span[0].archetype_index)
						break;
				}
				auto ite = span.subspan(0, index);
				span = span.subspan(index);
				std::array<std::size_t, ComponentManager::GetQueryDataCount()> query_data;
				auto re = manager.TranslateClassToQueryData(ite[0].archetype_index, { &entity_property_bitflag, 1 }, query_data);
				assert(re);
				while (!ite.empty())
				{
					auto re = manager.PopBackEntityToFillHole(*ite.begin(), *ite.rbegin());
					assert(re.has_value());
					if (*re)
					{
						void* output = nullptr;
						auto re = manager.QueryComponent(*ite.begin(), query_data, std::span<void*>{(&output), 1});
						EntityProperty* property = static_cast<EntityProperty*>(output);
						assert(re && property != nullptr && property->entity);
						{
							std::lock_guard lg(property->entity->mutex);
							property->entity->index = *ite.begin();
						}
						ite = ite.subspan(1);
					}
					else {
						ite = ite.subspan(0, ite.size() - 1);
					}
				}
			}
		}

		entity_modifier.clear();
		entity_modifier_event.clear();
		return has_been_update;
	}
}