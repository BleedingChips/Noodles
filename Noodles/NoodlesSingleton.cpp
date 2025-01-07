module;

#include <cassert>

module NoodlesSingleton;

namespace Noodles
{

	std::byte* SingletonView::GetSingleton(
		Archetype::MemberView const& view
	) const
	{
		if(buffer != nullptr)
			return buffer + view.offset;
		return nullptr;
	}
	std::byte* SingletonView::GetSingleton(
		Archetype::Index index
	) const
	{
		if(archetype)
			return GetSingleton(archetype->GetMemberView(index));
		return nullptr;
	}

	std::byte* SingletonView::GetSingleton(
		MarkIndex id
	) const
	{
		if(archetype)
		{
			auto loc = archetype->Locate(id);
			if(loc)
			{
				return this->GetSingleton(archetype->GetMemberView(*loc));
			}
		}
		return nullptr;
	}


	bool SingletonManager::Modify::Release()
	{
		if(resource)
		{
			struct_layout->Destruction(resource.Get());
			struct_layout.Reset();
			resource = {};
			return true;
		}
		return false;
	}
	
	SingletonManager::SingletonManager(StructLayoutManager& manager, Config config)
		: manager(&manager),
		singleton_resource(config.singleton_resource),
		modify_mask(config.resource),
		modifier(config.resource),
		singleton_mark(config.resource)
	{
		modify_mask.resize(manager.GetSingletonStorageCount());
		singleton_mark.resize(manager.GetSingletonStorageCount());
	}

	SingletonManager::~SingletonManager()
	{
		for(auto& ite : modifier)
		{
			ite.Release();
		}

		ClearCurrentSingleton_AssumedLocked();
	}

	bool SingletonManager::ReadSingleton_AssumedLocked(SingletonQuery const& query, QueryData& accessor) const
	{
		std::shared_lock lg(query.GetMutex());
		if (query.VersionCheck_AssumedLocked(reinterpret_cast<std::size_t>(singleton_archetype.GetPointer())))
		{
			auto span = query.EnumSingleton_AssumedLocked();
			auto view = GetSingletonView_AssumedLocked();
			std::size_t min = std::min(span.size(), accessor.output_buffer.size());
			accessor.array_size = 1;
			if (singleton_record)
			{
				for (std::size_t i = 0; i < min; ++i)
				{
					if (span[i] != std::numeric_limits<std::size_t>::max())
					{
						accessor.output_buffer[i] = singleton_record.GetByte(i);
					}else
					{
						accessor.output_buffer[i] = nullptr;
					}
				}
			}else
			{
				for (std::size_t i = 0; i < min; ++i)
				{
					accessor.output_buffer[i] = nullptr;
				}
			}
			return true;
		}
		return false;
	}

	bool SingletonManager::UpdateFilter_AssumedLocked(SingletonQuery& query) const
	{
		std::lock_guard lg(query.GetMutex());
		if (query.Update_AssumedLocked(*manager, reinterpret_cast<std::size_t>(singleton_archetype.GetPointer())))
		{
			if (singleton_archetype)
			{
				query.OnSingletonModify_AssumedLocked(*singleton_archetype);
			}
			return true;
		}
		return false;
	}

	bool SingletonManager::AddSingleton(StructLayout const& struct_layout, void* target_buffer, EntityManager::Operation operation, std::pmr::memory_resource* temp_resource)
	{
		auto loc = manager->LocateSingleton(struct_layout);
		
		if(loc.has_value())
		{
			std::lock_guard lg(modifier_mutex);
			auto re = MarkElement::CheckIsMark(modify_mask, *loc);
			if(!re)
			{
				auto record = Potato::IR::MemoryResourceRecord::Allocate(temp_resource, struct_layout.GetLayout());
				if(record)
				{
					if(operation == EntityManager::Operation::Move)
					{
						auto re = struct_layout.MoveConstruction(
							record.GetByte(),
							target_buffer
						);
						assert(re);
					}else
					{
						auto re = struct_layout.CopyConstruction(
							record.GetByte(),
							target_buffer
						);
						assert(re);
					}
					modifier.emplace_back(
						*loc,
						record,
						&struct_layout
					);
					re = MarkElement::Mark(modify_mask, *loc);
					assert(!re);
					return true;
				}
				
			}
		}
		return false;
	}

	bool SingletonManager::RemoveSingleton(StructLayout const& struct_layout)
	{
		auto loc = manager->LocateSingleton(struct_layout);
		if(loc)
		{
			std::lock_guard lg(modifier_mutex);
			auto re = MarkElement::Mark(modify_mask, *loc, false);
			if(re)
			{
				for(auto& ite : modifier)
				{
					if(ite.mark_index == *loc)
					{
						ite.Release();
						break;
					}
				}
				return true;
			}
		}
		return false;
	}

	bool SingletonManager::ClearCurrentSingleton_AssumedLocked()
	{
		if (singleton_record)
		{
			SingletonView cv
			{
			singleton_archetype,
			singleton_record.GetByte()
			};
			for (auto& ite : singleton_archetype->GetMemberView())
			{
				ite.layout->Destruction(
					cv.GetSingleton(ite)
				);
			}
			singleton_record = {};
			singleton_archetype.Reset();
			return true;
		}
		return false;
	}

	bool SingletonManager::Flush(std::pmr::memory_resource* temp_resource)
	{
		std::lock(mutex, modifier_mutex);
		has_singleton_update = false;
		std::lock_guard lg(mutex, std::adopt_lock);
		std::lock_guard lg2(modifier_mutex, std::adopt_lock);

		if(singleton_archetype && MarkElement::IsReset(modify_mask))
		{
			ClearCurrentSingleton_AssumedLocked();
			has_singleton_update = true;
			return has_singleton_update;
		}

		if(singleton_archetype && MarkElement::IsSame(singleton_archetype->GetAtomicTypeMark(), modify_mask))
		{
			auto View = GetSingletonView_AssumedLocked();
			for (auto& ite : modifier)
			{
				has_singleton_update = true;
				if (ite.resource)
				{
					auto mindex = singleton_archetype->Locate(ite.mark_index);
					assert(mindex);
					auto mm = singleton_archetype->GetMemberView(*mindex);
					auto target = View.GetSingleton(mm);
					assert(target != nullptr);
					mm.layout->Destruction(target);
					mm.layout->MoveConstruction(
						target,
						ite.resource.Get()
					);
					ite.Release();
				}
			}
			modifier.clear();
			return has_singleton_update;
		}
		if(!modifier.empty())
		{
			std::pmr::vector<Archetype::Init> init_list{ temp_resource };
			std::pmr::vector<MarkElement> marks{ temp_resource };
			marks.resize(manager->GetSingletonStorageCount());
			for(auto& ite : modifier)
			{
				if(ite.resource)
				{
					init_list.emplace_back(ite.struct_layout, ite.mark_index);
					auto re = MarkElement::Mark(marks, ite.mark_index);
					assert(!re);
				}
			}
			if(singleton_archetype)
			{
				for (auto& ite : singleton_archetype->GetMemberView())
				{
					auto re = MarkElement::CheckIsMark(modify_mask, ite.index);
					if(re)
					{
						auto re = MarkElement::Mark(marks, ite.index);
						if(!re)
						{
							init_list.emplace_back(ite.layout, ite.index);
						}
					}
				}
			}
			auto new_archetype = Archetype::Create(
				manager->GetSingletonStorageCount(),
				init_list,
				&singleton_resource
			);
			if(new_archetype)
			{
				auto record = Potato::IR::MemoryResourceRecord::Allocate(
					&singleton_resource,
					new_archetype->GetLayout()
				);
				if(record)
				{
					has_singleton_update = true;
					MarkElement::Reset(marks);
					auto new_view = SingletonView{
						new_archetype,
						record.GetByte()
					};
					
					for (auto& ite : modifier)
					{
						if (ite.resource)
						{
							auto mindex = new_archetype->Locate(ite.mark_index);
							assert(mindex);
							auto mm = new_archetype->GetMemberView(*mindex);
							auto tar = new_view.GetSingleton(mm);
							mm.layout->CopyConstruction(
								tar,
								ite.resource.Get()
							);
							ite.Release();
							auto re = MarkElement::Mark(marks, ite.mark_index);
							assert(!re);
						}
					}
					if(singleton_archetype)
					{
						auto old_view = GetSingletonView_AssumedLocked();
						for (auto& ite : singleton_archetype->GetMemberView())
						{
							auto source = old_view.GetSingleton(ite);
							auto re = MarkElement::CheckIsMark(modify_mask, ite.index);
							if (re)
							{
								re = MarkElement::Mark(marks, ite.index);
								if (!re)
								{
									auto mindex = new_archetype->Locate(ite.index);
									assert(mindex);
									auto mm = new_archetype->GetMemberView(*mindex);
									auto tar = new_view.GetSingleton(mm);
									
									mm.layout->CopyConstruction(
										tar,
										source
									);
								}
							}
							ite.layout->Destruction(source);
						}
					}
					modifier.clear();
					singleton_archetype = std::move(new_archetype);
					singleton_record = record;
					has_singleton_update = true;
					assert(singleton_archetype);
				}
			}
			MarkElement::CopyTo(modify_mask, singleton_mark);
		}
		return has_singleton_update;
	}
}