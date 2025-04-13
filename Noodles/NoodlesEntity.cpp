module;

#include <cassert>

module NoodlesEntity;
import NoodlesArchetype;

namespace Noodles
{
	void Entity::SetFree_AssumedLocked()
	{
		state = State::Free;
		component_index.archetype_index.Reset();
		modify_component_bitflag.Reset();
	}

	auto Entity::Create(GlobalContext& global_context, std::pmr::memory_resource* resource)
		-> Ptr
	{
		auto layout = Potato::MemLayout::MemLayoutCPP::Get<Entity>();
		auto element_count = global_context.GetComponentBitFlagContainerElementCount();

		auto offset_flag = layout.Insert(Potato::MemLayout::Layout::GetArray<BitFlagConstContainer::Element>(element_count * 2));
		auto record = Potato::IR::MemoryResourceRecord::Allocate(resource, layout.Get());

		if (record)
		{
			auto flag_span = std::span{
				new (record.GetByte(offset_flag)) BitFlagConstContainer::Element[element_count * 2],
				element_count * 2
			};

			for (auto& ite : flag_span)
			{
				ite = 0;
			}

			return new (record.Get()) Entity{
				record,
				BitFlagContainer{flag_span.subspan(0, element_count)},
				BitFlagContainer{flag_span.subspan(element_count)},
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

	EntityManager::EntityManager(GlobalContext::Ptr in_global_context, Config config)
		: global_context(std::move(in_global_context)), entity_modifier(config.resource), entity_modifier_event(config.resource)
	{
		assert(global_context);
		auto result = global_context->GetComponentBitFlag(*StructLayout::GetStatic<EntityProperty>());
		assert(result.has_value());
		entity_property_bitflag = *result;
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
		auto ent = Entity::Create(*global_context, entity_resource);
		if (ent)
		{
			EntityProperty entity_property
			{
				ent
			};

			auto re = AddEntityComponentImp(*ent, *StructLayout::GetStatic<EntityProperty>(), &entity_property, true, Operation::Move, temp_resource);
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

	bool EntityManager::AddEntityComponentImp(Entity& target_entity, StructLayout const& component_class, void* reference_buffer, bool accept_build_in, Operation operation, std::pmr::memory_resource* resource)
	{
		auto ope = component_class.GetOperateProperty();
		if (!ope.move_construct && !ope.copy_construct)
			return false;
		auto loc = global_context->GetComponentBitFlag(component_class);
		if (loc.has_value() && (accept_build_in || *loc != GetEntityPropertyBitFlag()))
		{
			std::lock_guard lg(target_entity.mutex);
			if (target_entity.state == Entity::State::Normal || target_entity.state == Entity::State::PreInit)
			{
				auto re = target_entity.modify_component_bitflag.SetValue(*loc);
				assert(re);
				if (!*re)
				{
					auto record = Potato::IR::MemoryResourceRecord::Allocate(resource, component_class.GetLayout());
					if (record)
					{
						if (operation == Operation::Copy)
						{
							auto re = component_class.CopyConstruction(record.Get(), reference_buffer);
							assert(re);
						}
						else
						{
							auto re = component_class.MoveConstruction(record.Get(), reference_buffer);
							assert(re);
						}

						decltype(entity_modifier_event)::iterator insert_ite = entity_modifier_event.end();

						if (!target_entity.modify_index)
						{
							target_entity.modify_index = entity_modifier.size();
							entity_modifier.emplace_back(
								false,
								Potato::Misc::IndexSpan<>{
								entity_modifier_event.size(),
									entity_modifier_event.size() + 1
							},
								& target_entity
							);
						}
						else
						{
							auto& ref = entity_modifier[target_entity.modify_index];
							insert_ite = ref.infos.End() + entity_modifier_event.begin();
							ref.infos.BackwardEnd(1);
						}

						entity_modifier_event.insert(
							insert_ite,
							EntityModifierEvent{
								true,
								*loc,
								& component_class,
								record
							}
						);

						for (auto i = target_entity.modify_index.Get() + 1; i < entity_modifier.size(); ++i)
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

	bool EntityManager::ReleaseEntity(Entity::Ptr target_entity)
	{
		assert(target_entity);
		std::lock_guard lg(target_entity->mutex);
		if (target_entity->state == Entity::State::Normal || target_entity->state == Entity::State::PreInit)
		{
			target_entity->state = Entity::State::PendingDestroy;
			if (!target_entity->modify_index)
			{
				target_entity->modify_index = entity_modifier.size();
				entity_modifier.emplace_back(
					true,
					Potato::Misc::IndexSpan<>{
					entity_modifier_event.size(),
						entity_modifier_event.size()
				},
					std::move(target_entity)
				);
			}
			else
			{
				auto& ref = entity_modifier[target_entity->modify_index];
				ref.need_remove = true;
				auto span_index = ref.infos;
				auto span = span_index.Slice(std::span(entity_modifier_event));
				for (auto& ite : span)
				{
					ite.Release();
				}
			}
			return true;
		}
		return false;
	}

	bool EntityManager::RemoveEntityComponentImp(Entity& target_entity, StructLayout const& component_class, bool accept_build_in)
	{
		auto loc = global_context->GetComponentBitFlag(component_class);
		if (loc.has_value() && (accept_build_in || *loc != GetEntityPropertyBitFlag()))
		{
			std::lock_guard lg(target_entity.mutex);
			if (target_entity.state == Entity::State::Normal || target_entity.state == Entity::State::PreInit)
			{
				auto re = target_entity.modify_component_bitflag.SetValue(*loc, false);
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
						if (ite.need_add && ite.bitflag == *loc)
						{
							ite.Release();
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

	bool EntityManager::Flush(ComponentManager& manager, std::pmr::memory_resource* temp_resource)
	{
		bool has_been_update = false;

		std::pmr::vector<ComponentManager::Index> removed_list(temp_resource);

		for (auto& ite : entity_modifier)
		{
			if (ite.entity && ite.need_remove)
			{
				has_been_update = true;
				auto& entity = *ite.entity;
				std::lock_guard lg(entity.mutex);
				assert(entity.state == Entity::State::PendingDestroy);
				if (entity.component_index)
				{
					manager.DestructionComponent(
						entity.component_index
					);
					removed_list.emplace_back(entity.component_index);
				}
				ite.entity.Reset();
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
					assert(entity.component_index);
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
					auto re = manager.ConstructComponent(entity.component_index, std::span(component_init_list), option);
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
						offset = manager.FlushComponentInitWithComponent(entity.component_index, entity.modify_component_bitflag, std::span(component_init_list), std::span(init_list));
					}
					else {
						offset = manager.FlushComponentInitWithComponent(entity.component_index, entity.modify_component_bitflag, std::span(component_init_list), {});
					}

					auto span = ite.infos.Slice(std::span{ entity_modifier_event });
					component_init_list.clear();
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
							component_init_list.emplace_back(ite2.bitflag, true, ite2.resource.GetByte());
						}
					}

					for (auto& ite : component_init_list)
					{
						ite.move_construct = true;
					}

					if (!archetype_index)
					{
						archetype_index = manager.CreateComponentChunk(std::span(init_list));
						assert(archetype_index);
						if (!archetype_index)
						{
							continue;
						}
					}

					auto new_component_index = manager.AllocateNewComponentWithoutConstruct(archetype_index);
					if (!new_component_index)
					{
						continue;
					}



					ComponentManager::ConstructOption option;
					option.destruct_before_construct = false;

					auto re = manager.ConstructComponent(new_component_index, std::span(component_init_list), option);
					assert(re);

					if (entity.component_index)
					{
						re = manager.DestructionComponent(entity.component_index);
						assert(re);
						removed_list.emplace_back(entity.component_index);
					}
					
					entity.component_index = new_component_index;
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

		entity_modifier.clear();
		entity_modifier_event.clear();
		return has_been_update;
	}

	/*
	

	

	

	bool EntityManager::ReadEntityComponents_AssumedLocked(ComponentManager const& manager, Entity const& ent, ComponentQuery const& query, QueryData& accessor) const
	{
		std::shared_lock sl(ent.mutex);
		if (ent.state == Entity::State::Normal && ent.archetype_index)
		{
			return manager.ReadComponent_AssumedLocked(
				ent.archetype_index,
				ent.column_index,
				query,
				accessor
			);
		}
		return false;
	}

	

		manager.FixComponentChunkHole_AssumedLocked(remove_list, [](void* data, ChunkView const& view, std::size_t from, std::size_t to)
		{
			auto entity_property = reinterpret_cast<EntityProperty*>(view.GetComponent(static_cast<EntityManager*>(data)->GetEntityPropertyAtomicTypeID(), to));
			assert(entity_property != nullptr);
			auto entity = entity_property->GetEntity();
			assert(entity);
			std::lock_guard lg(entity->mutex);
			entity->column_index = to;
		}, this);
		assert(remove_list.empty());

		return Updated;
	}
	*/
}