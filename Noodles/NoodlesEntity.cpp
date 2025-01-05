module;

#include <cassert>

module NoodlesEntity;

namespace Noodles
{

	void Entity::SetFree_AssumedLocked()
	{
		state = State::Free;
		archetype_index.Reset();
		modify_index.Reset();
		MarkElement::Reset(current_component_mask);
		MarkElement::Reset(modify_component_mask);
	}

	auto Entity::Create(StructLayoutManager const& manager, std::pmr::memory_resource* resource)
		-> Ptr
	{
		auto layout = Potato::MemLayout::MemLayoutCPP::Get<Entity>();
		auto offset_current_mask = layout.Insert(Potato::MemLayout::Layout::GetArray<MarkElement>(manager.GetComponentStorageCount()));
		auto offset_modify_component = layout.Insert(Potato::MemLayout::Layout::GetArray<MarkElement>(manager.GetComponentStorageCount()));

		auto record = Potato::IR::MemoryResourceRecord::Allocate(resource, layout.Get());
		if (record)
		{
			auto current_mask = std::span{
				new (record.GetByte(offset_current_mask)) MarkElement[manager.GetComponentStorageCount()],
				manager.GetComponentStorageCount()
			};
			auto modify_component = std::span{
				new (record.GetByte(offset_modify_component)) MarkElement[manager.GetComponentStorageCount()],
				manager.GetComponentStorageCount()
			};
			return new (record.Get()) Entity{
				record,
				current_mask,
				modify_component
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

	bool EntityManager::ReadEntityComponents_AssumedLocked(ComponentManager const& manager, Entity const& ent, ComponentFilter const& filter, ComponentAccessor& accessor) const
	{
		std::shared_lock sl(ent.mutex);
		if (ent.state == Entity::State::Normal && ent.archetype_index)
		{
			return manager.ReadComponent_AssumedLocked(
				ent.archetype_index,
				ent.column_index,
				filter,
				accessor
			);
		}
		return false;
	}

	EntityManager::EntityManager(StructLayoutManager& manager, Config config)
		: entity_entity_property_index(*manager.LocateComponent(*StructLayout::GetStatic<EntityProperty>())), manager(&manager), entity_modifier(config.resource), entity_modifier_event(config.resource)
	{

	}

	EntityManager::~EntityManager()
	{
		for(auto& ite : entity_modifier_event)
		{
			ite.Release();
		}
		entity_modifier_event.clear();
	}

	Entity::Ptr EntityManager::CreateEntity(std::pmr::memory_resource* entity_resource, std::pmr::memory_resource* temp_resource)
	{
		auto ent = Entity::Create(*manager, entity_resource);
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

	/*
	bool EntityManager::Init(ComponentManager& manager)
	{
		auto re = manager.LocateStructLayout(*StructLayout::GetStatic<EntityProperty>());
		if(re && re->index == 0)
		{
			return true;
		}
		assert(false);
		return false;
	}
	*/

	bool EntityManager::EntityModifierEvent::Release()
	{
		if(resource.Get() != nullptr)
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

	bool EntityManager::AddEntityComponentImp(Entity& target_entity, StructLayout const& atomic_type, void* reference_buffer, bool accept_build_in, Operation operation, std::pmr::memory_resource* resource)
	{
		auto ope = atomic_type.GetOperateProperty();
		if (!ope.move_construct && !ope.copy_construct)
			return false;
		auto loc = manager->LocateComponent(atomic_type);
		if (loc.has_value() && (accept_build_in || *loc != GetEntityPropertyAtomicTypeID()))
		{
			std::lock_guard lg(target_entity.mutex);
			if (target_entity.state == Entity::State::Normal || target_entity.state == Entity::State::PreInit)
			{
				auto re = MarkElement::Mark(target_entity.modify_component_mask, *loc);
				if (!re)
				{
					auto record = Potato::IR::MemoryResourceRecord::Allocate(resource, atomic_type.GetLayout());
					if (record)
					{
						if (operation == Operation::Copy)
						{
							auto re = atomic_type.CopyConstruction(record.Get(), reference_buffer);
							assert(re);
						}
						else
						{
							auto re = atomic_type.MoveConstruction(record.Get(), reference_buffer);
							assert(re);
						}

						std::lock_guard lg(entity_mutex);

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
								&target_entity
							);
						}else
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
								&atomic_type,
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
			std::lock_guard lg(entity_mutex);
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
				for(auto& ite : span)
				{
					ite.Release();
				}
			}
			return true;
		}
		return false;
	}

	bool EntityManager::RemoveEntityComponentImp(Entity& target_entity, StructLayout const& atomic_type, bool accept_build_in)
	{
		auto loc = manager->LocateComponent(atomic_type);
		if (loc.has_value() && (accept_build_in || *loc != GetEntityPropertyAtomicTypeID()))
		{
			std::lock_guard lg(target_entity.mutex);
			if (target_entity.state == Entity::State::Normal || target_entity.state == Entity::State::PreInit)
			{

				auto re = MarkElement::Mark(target_entity.modify_component_mask, *loc, false);
				if(re)
				{
					Potato::Misc::IndexSpan<> event_span;

					std::lock_guard lg(entity_mutex);

					if (!target_entity.modify_index)
					{
						target_entity.modify_index = entity_modifier.size();
						entity_modifier.emplace_back(
							false,
							Potato::Misc::IndexSpan<>{
							entity_modifier_event.size(),
								entity_modifier_event.size()
						},
							&target_entity
						);
						event_span = { entity_modifier_event.size(), entity_modifier_event.size() };
					}
					else
					{
						event_span = entity_modifier[target_entity.modify_index].infos;
					}
					auto span = event_span.Slice(std::span(entity_modifier_event));
					for(auto& ite : span)
					{
						if(ite.need_add && ite.index == *loc)
						{
							ite.Release();
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
		bool Updated = false;

		std::pmr::vector<ComponentManager::RemovedColumn> remove_list(temp_resource);

		std::lock_guard lg(manager.GetMutex());

		{
			std::lock_guard lg(entity_mutex);

			for (auto& ite : entity_modifier)
			{
				if (ite.entity && ite.need_remove)
				{
					Updated = true;
					auto& entity = *ite.entity;
					std::lock_guard lg(entity.mutex);
					assert(entity.state == Entity::State::PendingDestroy);
					if (entity.archetype_index)
					{
						manager.ReleaseComponentColumn_AssumedLocked(
							entity.archetype_index,
							entity.column_index,
							remove_list
						);
					}
					ite.entity.Reset();
				}
			}

			ComponentManager::ArchetypeBuilderRef builder{ manager, temp_resource };

			for (auto& ite : entity_modifier)
			{
				if (ite.entity)
				{
					Updated = true;
					auto& entity = *ite.entity;
					std::lock_guard lg(entity.mutex);
					if (MarkElement::IsSame(entity.modify_component_mask, entity.current_component_mask))
					{
						assert(entity.archetype_index);
						auto chunk = manager.GetChunk_AssumedLocked(entity.archetype_index);
						auto span = ite.infos.Slice(std::span{ entity_modifier_event });
						for (auto& ite2 : span)
						{
							if (ite2.need_add)
							{
								auto mindex = chunk.archetype->Locate(ite2.index);
								assert(mindex);
								auto buffer = chunk.GetComponent(*mindex, entity.column_index);
								auto& mview = chunk.archetype->GetMemberView(*mindex);
								mview.layout->Destruction(
									buffer
								);
								mview.layout->MoveConstruction(
									buffer,
									ite2.resource.Get()
								);
							}
						}
					}
					else
					{
						auto [archetype, archetype_index] = manager.FindArchetype_AssumedLocked(
							entity.modify_component_mask
						);

						if (!archetype)
						{
							builder.Clear();
							auto span = ite.infos.Slice(std::span{ entity_modifier_event });
							for (auto& ite2 : span)
							{
								if (ite2.need_add)
								{
									auto re = builder.Insert(ite2.struct_layout, ite2.index);
									assert(re);
								}
							}

							if (entity.archetype_index)
							{
								auto mm = manager.GetChunk_AssumedLocked(entity.archetype_index);
								for (auto& ite2 : mm.archetype->GetMemberView())
								{
									auto re = MarkElement::CheckIsMark(entity.modify_component_mask, ite2.index);
									if (re)
									{
										builder.Insert(ite2.layout, ite2.index);
									}
								}
							}

							assert(MarkElement::IsSame(builder.GetMarks(), entity.modify_component_mask));

							std::tie(archetype, archetype_index) = manager.FindOrCreateArchetype_AssumedLocked(builder);

							assert(archetype && archetype_index);
						}

						auto index = manager.AllocateComponentColumn_AssumedLocked(archetype_index, remove_list);

						assert(index);

						auto mm = manager.GetChunk_AssumedLocked(archetype_index);
						builder.Clear();

						auto span = ite.infos.Slice(std::span{ entity_modifier_event });
						for (auto& ite2 : span)
						{
							if (ite2.need_add)
							{
								auto re = mm.MoveConstructComponent(
									ite2.index,
									index,
									ite2.resource.GetByte()
								);
								assert(re);
								auto re2 = MarkElement::Mark(builder.GetMarks(), ite2.index);
								assert(!re2);
							}
						}

						if (entity.archetype_index)
						{
							auto old_mm = manager.GetChunk_AssumedLocked(entity.archetype_index);
							assert(old_mm);
							for (auto& ite2 : old_mm.archetype->GetMemberView())
							{
								auto re = MarkElement::CheckIsMark(entity.modify_component_mask, ite2.index);
								if (re)
								{
									re = MarkElement::Mark(builder.GetMarks(), ite2.index);
									if (!re)
									{
										auto target_buffer = old_mm.GetComponent(
											ite2.index,
											entity.column_index
										);

										assert(target_buffer != nullptr);

										auto re2 = mm.MoveConstructComponent(
											ite2.index,
											index,
											target_buffer
										);

										assert(re2);
									}
								}
							}
							auto re = manager.ReleaseComponentColumn_AssumedLocked(entity.archetype_index, entity.column_index, remove_list);
							assert(re);
						}

						entity.state = Entity::State::Normal;
						entity.modify_index.Reset();
						entity.archetype_index = archetype_index;
						entity.column_index = index;
						MarkElement::CopyTo(entity.modify_component_mask, entity.current_component_mask);
					}
				}
			}

			for(auto& ite : entity_modifier_event)
			{
				ite.Release();
			}

			entity_modifier.clear();
			entity_modifier_event.clear();
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
}