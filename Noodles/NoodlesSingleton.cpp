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


	SingletonFilter::Ptr SingletonFilter::Create(
		StructLayoutMarkIndexManager& manager,
		std::span<Info const> require_singleton,
		std::pmr::memory_resource* storage_resource
	)
	{
		auto layout = Potato::MemLayout::MemLayoutCPP::Get<SingletonFilter>();
		auto require_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<MarkElement>(manager.GetStorageCount()));
		auto require_write_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<MarkElement>(manager.GetStorageCount()));
		auto atomic_index_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<MarkIndex>(require_singleton.size()));
		auto reference_offset = layout.Insert(Potato::MemLayout::Layout::GetArray<std::size_t>(require_singleton.size()));
		auto re = Potato::IR::MemoryResourceRecord::Allocate(storage_resource, layout.Get());
		if (re)
		{
			std::span<MarkElement> require_mask{ new (re.GetByte(require_offset)) MarkElement[manager.GetStorageCount()], manager.GetStorageCount() };
			std::span<MarkElement> require_write_mask{ new (re.GetByte(require_write_offset)) MarkElement[manager.GetStorageCount()], manager.GetStorageCount() };
			std::span<MarkIndex> require_index{ new (re.GetByte(atomic_index_offset)) MarkIndex[require_singleton.size()], require_singleton.size() };
			std::span<std::size_t> reference_span{ new (re.GetByte(reference_offset)) std::size_t[require_singleton.size()], require_singleton.size() };

			for (auto& ite : reference_span)
			{
				ite = std::numeric_limits<std::size_t>::max();
			}

			auto ite_span = require_index;

			for (auto& Ite : require_singleton)
			{
				auto loc = manager.LocateOrAdd(Ite.struct_layout);
				if (loc.has_value())
				{
					MarkElement::Mark(require_mask, *loc);
					ite_span[0] = *loc;
					ite_span = ite_span.subspan(1);
					if (Ite.need_write)
					{
						MarkElement::Mark(require_write_mask, *loc);
					}
				}
				else
				{
					assert(false);
				}
			}

			return new (re.Get()) SingletonFilter{ re, require_mask, require_write_mask, require_index, reference_span };
		}
		return {};
	}

	bool SingletonFilter::OnSingletonModify(Archetype const& archetype)
	{
		std::lock_guard lg(mutex);
		auto span = GetMarkIndex();
		auto ref = archetype_offset;
		for (auto& ite : span)
		{
			auto loc = archetype.Locate(ite);
			if (loc.has_value())
			{
				ref[0] = archetype[loc->index].offset;
			}
			else
			{
				ref[0] = std::numeric_limits<std::size_t>::max();
			}
			ref = ref.subspan(1);
		}
		return true;
	}

	void SingletonFilter::Reset()
	{
		std::lock_guard lg(mutex);
		for(auto& ite : archetype_offset)
		{
			ite = std::numeric_limits<std::size_t>::max();
		}
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
	
	SingletonManager::SingletonManager(Config config)
		: manager(config.singleton_max_atomic_count, config.resource),
		singleton_resource(config.singleton_resource),
		filter(config.resource),
		modify_mask(config.resource),
		modifier(config.resource)
	{
		modify_mask.resize(manager.GetStorageCount());
	}

	SingletonManager::~SingletonManager()
	{
		for(auto& ite : modifier)
		{
			ite.Release();
		}

		ClearCurrentSingleton_AssumedLocked();
	}

	SingletonFilter::Ptr SingletonManager::CreateSingletonFilter(std::span<SingletonFilter::Info const> require_atomic, std::size_t identity, std::pmr::memory_resource* filter_resource)
	{
		auto ptr = SingletonFilter::Create(
			manager,
			require_atomic,
			filter_resource
		);
		if(ptr)
		{
			if(singleton_archetype)
			{
				ptr->OnSingletonModify(*singleton_archetype);
			}
			
			std::lock_guard lg(filter_mutex);
			filter.emplace_back(
				ptr,
				identity
			);

		}
		return ptr;
	}

	SingletonWrapper SingletonManager::ReadSingleton_AssumedLocked(SingletonFilter const& filter, std::pmr::memory_resource* wrapper_resource) const
	{
		std::pmr::vector<std::byte*> buffers{wrapper_resource};
		std::shared_lock sl(filter.mutex);
		auto span = filter.EnumSingleton_AssumedLocked();
		buffers.reserve(span.size());
		auto view = GetSingletonView_AssumedLocked();
		for(auto ite : span)
		{
			if (ite != std::numeric_limits<std::size_t>::max() && singleton_record)
			{
				buffers.push_back(
					singleton_record.GetByte(ite)
				);
			}
			else
			{
				buffers.push_back(
					nullptr
				);

			}
		}
		return {std::move(buffers)};
	}

	bool SingletonManager::AddSingleton(StructLayout::Ptr struct_layout, void* target_buffer, EntityManager::Operation operation, std::pmr::memory_resource* temp_resource)
	{
		auto loc = manager.LocateOrAdd(struct_layout);
		
		if(loc.has_value())
		{
			std::lock_guard lg(modifier_mutex);
			auto re = MarkElement::CheckIsMark(modify_mask, *loc);
			if(!re)
			{
				auto record = Potato::IR::MemoryResourceRecord::Allocate(temp_resource, struct_layout->GetLayout());
				if(record)
				{
					if(operation == EntityManager::Operation::Move)
					{
						auto re = struct_layout->MoveConstruction(
							record.GetByte(),
							target_buffer
						);
						assert(re);
					}else
					{
						auto re = struct_layout->CopyConstruction(
							record.GetByte(),
							target_buffer
						);
						assert(re);
					}
					modifier.emplace_back(
						*loc,
						record,
						std::move(struct_layout)
					);
					re = MarkElement::Mark(modify_mask, *loc);
					assert(!re);
					return true;
				}
				
			}
		}
		return false;
	}

	bool SingletonManager::RemoveSingleton(StructLayout::Ptr const& struct_layout)
	{
		auto loc = manager.LocateOrAdd(struct_layout);
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
		bool update = false;
		std::lock(mutex, modifier_mutex);
		std::lock_guard lg(mutex, std::adopt_lock);
		std::lock_guard lg2(modifier_mutex, std::adopt_lock);

		if(singleton_archetype && MarkElement::IsReset(modify_mask))
		{
			ClearCurrentSingleton_AssumedLocked();
			update = true;
			std::shared_lock sl(filter_mutex);
			for(auto& ite : filter)
			{
				ite.filter->Reset();
			}
			return update;
		}

		if(singleton_archetype && MarkElement::IsSame(singleton_archetype->GetAtomicTypeMark(), modify_mask))
		{
			auto View = GetSingletonView_AssumedLocked();
			for (auto& ite : modifier)
			{
				update = true;
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
			return update;
		}
		if(!modifier.empty())
		{
			std::pmr::vector<Archetype::Init> init_list{ temp_resource };
			std::pmr::vector<MarkElement> marks{ temp_resource };
			marks.resize(manager.GetStorageCount());
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
				manager,
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
					update = true;
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
					update = true;
					assert(singleton_archetype);
					std::shared_lock lg(filter_mutex);
					for (auto& ite : filter)
					{
						if (ite.identity)
						{
							ite.filter->OnSingletonModify(*singleton_archetype);
						}
					}
				}
			}
		}
		return update;
	}
}