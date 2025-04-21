module;

#include <cassert>

module NoodlesSingleton;

import NoodlesComponent;

namespace Noodles
{

	SingletonManager::SingletonManager(std::size_t singleton_container_count, Config config)
		: bitflag(singleton_container_count * 2, config.resource), singleton_resource(config.singleton_resource)
	{
		singleton_usage_bitflag = bitflag.AsSpan().subspan(0, singleton_container_count);
		singleton_update_bitflag = bitflag.AsSpan().subspan(singleton_container_count, singleton_container_count);
	}

	SingletonManager::~SingletonManager()
	{
		if (singleton_record)
		{
			assert(singleton_archetype);
			for (Archetype::MemberView const& ite : *singleton_archetype)
			{
				ite.struct_layout->Destruction(singleton_record.GetByte(ite.offset));
			}
			singleton_record.Deallocate();
			singleton_record = {};
			singleton_archetype.Reset();
		}
	}

	std::size_t SingletonManager::TranslateBitFlagToQueryData(std::span<BitFlag const> bitflag, std::span<std::size_t> output)
	{
		std::size_t index = 0;
		if (singleton_archetype && output.size() >= bitflag.size())
		{
			for (auto ite : bitflag)
			{
				auto mv = singleton_archetype->FindMemberIndex(ite);
				if (mv)
				{
					++index;
					output[0] = singleton_archetype->GetMemberView(mv.Get()).offset;
				}
				else {
					output[0] = std::numeric_limits<std::size_t>::max();
				}
				output = output.subspan(1);
			}
		}
		return index;
	}

	std::size_t SingletonManager::QuerySingletonData(std::span<std::size_t> query_data, std::span<void*> output_singleton)
	{
		std::size_t result = 0;
		if (singleton_archetype && output_singleton.size() >= query_data.size())
		{
			while (!query_data.empty())
			{
				if (query_data[0] != std::numeric_limits<std::size_t>::max())
				{
					++result;
					output_singleton[0] = singleton_record.GetByte(query_data[0]);
				}
				else {
					output_singleton[0] = nullptr;
				}
				query_data = query_data.subspan(1);
				output_singleton = output_singleton.subspan(1);
			}
		}
		return result;
	}



	bool SingletonModifyManager::Modify::Release()
	{
		if (resource)
		{
			singleton_class->Destruction(resource.Get());
			singleton_class.Reset();
			resource.Deallocate();
			resource = {};
			return true;
		}
		return false;
	}

	SingletonModifyManager::SingletonModifyManager(std::size_t singleton_container_count, std::pmr::memory_resource* resource)
		: singleton_modify(resource), singleton_modify_bitflag(singleton_container_count, resource)
	{

	}

	bool SingletonModifyManager::AddSingleton(StructLayout const& singleton_class, BitFlag singleton_bitflag, bool is_move_construct, void* singleton_data, std::pmr::memory_resource* resource)
	{
		BitFlagContainerViewer viwer = singleton_modify_bitflag;

		auto old_value = viwer.GetValue(singleton_bitflag);
		if (old_value.has_value() && !*old_value)
		{
			auto record = Potato::IR::MemoryResourceRecord::Allocate(resource, singleton_class.GetLayout());
			if (record)
			{
				if (is_move_construct)
				{
					auto re = singleton_class.MoveConstruction(record.Get(), singleton_data);
					assert(re);
				}
				else {
					auto re = singleton_class.CopyConstruction(record.Get(), singleton_data);
					assert(re);
				}

				singleton_modify.emplace_back(
					singleton_bitflag,
					record,
					&singleton_class
				);

				auto re = viwer.SetValue(singleton_bitflag);
				assert(re && !*re);
				return true;
			}
		}
		return false;
	}

	bool SingletonModifyManager::RemoveSingleton(BitFlag singleton_bitflag)
	{

		BitFlagContainerViewer viewer = singleton_modify_bitflag;

		auto re = viewer.GetValue(singleton_bitflag);
		if (re.has_value() && *re)
		{
			viewer.SetValue(singleton_bitflag, false);
			auto find = std::find_if(singleton_modify.begin(), singleton_modify.end(), [=](Modify const& i1) {
				return i1.singleton_bitflag == singleton_bitflag;
				});
			if (find != singleton_modify.end())
			{
				find->Release();
				singleton_modify.erase(find);
			}
			return true;
		}
		return false;
	}

	bool SingletonModifyManager::FlushSingletonModify(SingletonManager& manager, std::pmr::memory_resource* temp_resource)
	{
		BitFlagContainerViewer viewer = singleton_modify_bitflag;
		bool done = false;
		if (
			!singleton_modify.empty() || 
			!viewer.IsSame(manager.GetSingletonUsageBitFlagViewer())
			)
		{
			if (!viewer.IsSame(manager.GetSingletonUsageBitFlagViewer()))
			{
				if (!viewer.IsReset())
				{
					std::pmr::vector<Archetype::Init> init_list(temp_resource);
					std::pmr::vector<ComponentManager::Init> component_init_list(temp_resource);
					init_list.reserve(viewer.GetBitFlagCount());
					component_init_list.reserve(viewer.GetBitFlagCount());

					if (manager.singleton_archetype)
					{
						for (Archetype::MemberView const& ite : *manager.singleton_archetype)
						{
							auto re = viewer.GetValue(ite.bitflag);
							assert(re.has_value());
							if (*re)
							{
								init_list.emplace_back(ite.struct_layout, ite.bitflag);
								component_init_list.emplace_back(ite.bitflag, true, manager.singleton_record.GetByte(ite.offset));
							}
						}
					}

					for (auto& ite : singleton_modify)
					{
						auto find = std::find_if(component_init_list.begin(), component_init_list.end(), [&](ComponentManager::Init& init) {
							return init.component_class == ite.singleton_bitflag;
							});
						if (find != component_init_list.end())
						{
							find->data = ite.resource.Get();
						}
						else {
							init_list.emplace_back(ite.singleton_class, ite.singleton_bitflag);
							component_init_list.emplace_back(ite.singleton_bitflag, true, ite.resource.Get());
						}
					}

					ComponentManager::Sort(init_list);

					auto archetype = Archetype::Create(singleton_modify_bitflag.AsSpan().size(), init_list, &manager.singleton_resource);

					if (archetype)
					{
						auto new_record = Potato::IR::MemoryResourceRecord::Allocate(&manager.singleton_resource, archetype->GetLayout());
						if (new_record)
						{
							for (auto& ite : component_init_list)
							{
								auto loc = archetype->FindMemberIndex(ite.component_class);
								assert(loc);
								auto& mm = (*archetype)[loc.Get()];
								auto re = mm.struct_layout->MoveConstruction(new_record.GetByte(mm.offset), ite.data);
								assert(re);
							}

							if (manager.singleton_archetype)
							{
								for (Archetype::MemberView const& ite : *manager.singleton_archetype)
								{
									ite.struct_layout->Destruction(
										manager.singleton_record.GetByte(ite.offset)
									);
								}
								manager.singleton_record.Deallocate();
								manager.singleton_record = {};
							}


							manager.singleton_archetype = std::move(archetype);
							manager.singleton_record = new_record;
							done = true;
						}
					}
				}
				else {
					assert(manager.singleton_archetype);
					for (Archetype::MemberView const& ite : *manager.singleton_archetype)
					{
						ite.struct_layout->Destruction(
							manager.singleton_record.GetByte(ite.offset)
						);
					}
					done = true;
				}
				
			}
			else {
				assert(manager.singleton_archetype);
				for (auto& ite : singleton_modify)
				{
					auto loc = manager.singleton_archetype->FindMemberIndex(ite.singleton_bitflag);
					assert(loc);
					Archetype::MemberView const& mv = manager.singleton_archetype->GetMemberView()[loc.Get()];
					auto re = mv.struct_layout->Destruction(
						manager.singleton_record.GetByte(mv.offset)
					);
					assert(re);
					re = mv.struct_layout->MoveConstruction(
						manager.singleton_record.GetByte(mv.offset),
						ite.resource.Get()
						);
					assert(re);
				}
				done = true;
			}

			if (done)
			{
				++manager.version;
				auto re = manager.singleton_update_bitflag.ExclusiveOr(manager.singleton_usage_bitflag, singleton_modify_bitflag);
				assert(re);
				re = manager.singleton_usage_bitflag.CopyFrom(singleton_modify_bitflag);
				for (auto& ite : singleton_modify)
				{
					ite.Release();
				}
				singleton_modify.clear();
				return true;
			}
		}
		return false;
	}


	/*
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
	*/
}