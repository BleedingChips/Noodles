module;

#include <cassert>

module NoodlesSystem;
import PotatoFormat;

namespace Noodles
{

	std::strong_ordering SystemPriority::ComparePriority(SystemPriority const& p2) const
	{
		return Potato::Misc::PriorityCompareStrongOrdering(
			primary_priority, p2.primary_priority,
			second_priority, p2.second_priority
		);
	}

	std::partial_ordering Reversal(std::partial_ordering r)
	{
		if(r == std::partial_ordering::greater)
			return std::partial_ordering::less;
		else if(r == std::partial_ordering::less)
			return std::partial_ordering::greater;
		else
			return r;
	}

	std::partial_ordering SystemPriority::CompareCustomPriority(SystemProperty const& self_property, SystemPriority const& target, SystemProperty const& target_property) const
	{
		auto r0 = ComparePriority(target);
		if(r0 != std::strong_ordering::equal)
		{
			return static_cast<std::partial_ordering>(r0);
		}

		std::partial_ordering r1 = std::partial_ordering::unordered;
		std::partial_ordering r2 = std::partial_ordering::unordered;

		if(compare != nullptr)
		{
			r1 = (*compare)(self_property, target_property);
		}

		if(target.compare != nullptr)
		{
			r2 = (*target.compare)(target_property, self_property);
		}

		if(r1 == std::partial_ordering::unordered)
			return Reversal(r2);
		else if(r1 == std::partial_ordering::equivalent)
		{
			if (r2 != std::partial_ordering::unordered)
				return Reversal(r2);
			else
				return std::partial_ordering::equivalent;
		}else
		{
			if (r1 != r2)
				return r1;
			else
				return std::partial_ordering::unordered;
		}
	}

	bool DetectConflict(std::span<SystemRWInfo const> t1, std::span<SystemRWInfo const> t2)
	{
		auto ite1 = t1.begin();
		auto ite2 = t2.begin();

		while(ite1 != t1.end() && ite2 != t2.end())
		{
			auto re = ite1->type_id <=> ite2->type_id;
			if(re == std::strong_ordering::equal)
			{
				if(
					ite1->is_write
					|| ite2->is_write
					)
				{
					return true;
				}else
				{
					++ite2; ++ite1;
				}
			}else if(re == std::strong_ordering::less)
			{
				++ite1;	
			}else
			{
				++ite2;
			}
		}
		return false;
	}

	bool SystemMutex::IsConflict(SystemMutex const& p2) const
	{
		auto re1 = DetectConflict(component_rw_infos, p2.component_rw_infos);
		return re1;
	}

	auto SystemComponentFilter::Create(
		std::span<SystemRWInfo const> inf,
		std::pmr::memory_resource* upstream
	) -> Ptr
	{
		static_assert(alignof(SystemComponentFilter) == alignof(SystemRWInfo));
		if (upstream != nullptr)
		{
			std::size_t allocate_size = sizeof(SystemComponentFilter) + inf.size() * sizeof(SystemRWInfo);
			auto data = upstream->allocate(
					allocate_size,
				alignof(SystemComponentFilter)
			);
			if(data != nullptr)
			{
				Ptr ptr = new (data) SystemComponentFilter{
					std::span<std::byte>{
						reinterpret_cast<std::byte*>(static_cast<SystemComponentFilter*>(data) + 1),
						allocate_size - sizeof(SystemComponentFilter)
					},
					allocate_size,
					inf,
					upstream
				};
				return ptr;
			}
		}
		return {};
	}

	bool SystemComponentFilter::TryPreCollection(std::size_t element_index, Archetype const& archetype)
	{
		std::lock_guard lg(mutex);
		auto cache_size = id_index.size();
		std::size_t max_size = archetype.GetTypeIDCount();
		for(auto& ite : ref_infos)
		{
			auto locate_index = archetype.LocateTypeID(ite.type_id);
			if (locate_index.has_value())
			{
				id_index.push_back({ locate_index->index, locate_index->count });
			}
			else
			{
				id_index.resize(cache_size);
				return false;
			}
		}
		in_direct_mapping.push_back(
			{
			element_index,
			cache_size
			}
		);
		return true;
	}

	SystemComponentFilter::SystemComponentFilter(
		std::span<std::byte> append_buffer, std::size_t allocate_size, 
		std::span<SystemRWInfo const> ref, std::pmr::memory_resource* resource
		) : in_direct_mapping(resource), id_index(resource), allocate_size(allocate_size), resource(resource)
	{
		assert(append_buffer.size() >= ref.size() * sizeof(SystemRWInfo));
		std::span<SystemRWInfo> ite_span = {
			reinterpret_cast<SystemRWInfo*>(append_buffer.data()),
			ref.size()
		};
		for(std::size_t i = 0; i < ref.size(); ++i)
		{
			new (&ite_span[i]) SystemRWInfo{ref[i]};
		}
		ref_infos = ite_span;
	}

	SystemComponentFilter::~SystemComponentFilter()
	{
		for(auto& ite : ref_infos)
		{
			ite.~SystemRWInfo();
		}
		ref_infos = {};
	}

	void SystemComponentFilter::AddRef() const { Ref.AddRef(); }
	void SystemComponentFilter::SubRef() const
	{
		if(Ref.SubRef())
		{
			auto old_resource = resource;
			auto old_size = allocate_size;
			this->~SystemComponentFilter();
			old_resource->deallocate(const_cast<SystemComponentFilter*>(this), old_size, alignof(SystemComponentFilter));
		}
	} 

	auto SystemEntityFilter::Create(std::span<SystemRWInfo const> infos, std::pmr::memory_resource* resource)
		-> Ptr
	{
		static_assert(alignof(SystemEntityFilter) == alignof(SystemRWInfo));
		if (resource != nullptr)
		{
			std::size_t allocate_size = sizeof(SystemEntityFilter) + infos.size() * sizeof(SystemRWInfo);
			auto data = resource->allocate(
				allocate_size,
				alignof(SystemEntityFilter)
			);
			if (data != nullptr)
			{
				Ptr ptr = new (data) SystemEntityFilter{
					resource,
					allocate_size,
					infos,
					std::span<std::byte>{
						reinterpret_cast<std::byte*>(static_cast<SystemEntityFilter*>(data) + 1),
						allocate_size - sizeof(SystemEntityFilter)
					}
				};
				return ptr;
			}
		}
		return {};
	}

	SystemEntityFilter::SystemEntityFilter(std::pmr::memory_resource* up_stream, std::size_t allocated_size,
		std::span<SystemRWInfo const> ref, std::span<std::byte> buffer)
			: resource(up_stream), allocate_size(allocated_size)
	{
		assert(buffer.size() >= ref.size() * sizeof(SystemRWInfo));
		std::span<SystemRWInfo> ite_span = {
			reinterpret_cast<SystemRWInfo*>(buffer.data()),
			ref.size()
		};
		for (std::size_t i = 0; i < ref.size(); ++i)
		{
			new (&ite_span[i]) SystemRWInfo{ ref[i] };
		}
		ref_infos = ite_span;
	}

	SystemEntityFilter::~SystemEntityFilter()
	{
		for (auto& ite : ref_infos)
		{
			ite.~SystemRWInfo();
		}
	}

	void SystemEntityFilter::Release()
	{
		auto old_resource = resource;
		auto old_size = allocate_size;
		this->~SystemEntityFilter();
		old_resource->deallocate(const_cast<SystemEntityFilter*>(this), old_size, alignof(SystemEntityFilter));
	}

	SystemComponentFilter::Ptr FilterGenerator::CreateComponentFilter(std::span<SystemRWInfo const> rw_infos)
	{
		auto ptr = SystemComponentFilter::Create(rw_infos, system_resource);
		if(ptr)
		{
			need_register_component.push_back(ptr);
			for(auto& ite : rw_infos)
			{
				auto find = std::find_if(
					component_rw_infos.begin(),
					component_rw_infos.end(),
					[&](SystemRWInfo const& ite2)
					{
						return ite.type_id >= ite2.type_id;
					}
				);

				if(find == component_rw_infos.end())
				{
					component_rw_infos.push_back(ite);
				}else if(ite.type_id == find->type_id)
				{
					find->is_write = (find->is_write || ite.is_write);
				}
			}
			return ptr;
		}
		return {};
	}


	SystemEntityFilter::Ptr FilterGenerator::CreateEntityFilter(std::span<SystemRWInfo const> rw_infos)
	{
		auto ptr = SystemEntityFilter::Create(rw_infos, system_resource);
		if (ptr)
		{
			for (auto& ite : rw_infos)
			{
				auto find = std::find_if(
					component_rw_infos.begin(),
					component_rw_infos.end(),
					[&](SystemRWInfo const& ite2)
					{
						return ite.type_id == ite2.type_id;
					}
				);
				if (find == component_rw_infos.end())
				{
					component_rw_infos.push_back(ite);
				}
				else
				{
					find->is_write = (find->is_write || ite.is_write);
				}
			}
			return ptr;
		}
		return {};
	}

	TickSystemsGroup::TickSystemsGroup(std::pmr::memory_resource* resource)
		: system_holder_resource(resource), graphic_node(resource),
		system_contexts(resource),
		tick_systems_running_graphic_line(resource),
		startup_system(resource)
	{
		
	}

	SystemRegisterResult TickSystemsGroup::PreRegisterCheck(std::int32_t layer, SystemPriority priority, SystemProperty property, SystemMutex const& mutex, std::pmr::vector<SystemTemporaryDependenceLine>& dep)
	{
		std::pmr::vector<SystemTemporaryDependenceLine> TDLine;

		if (!property.system_name.empty())
		{
			bool has_to_node = false;
			std::size_t in_degree = 0;

			auto find = std::find_if(
				graphic_node.begin(),
				graphic_node.end(),
				[=](StorageSystemHolder const& C)
				{
					return C.layer >= layer;
				}
			);

			auto find2 = std::find_if(
				find,
				graphic_node.end(),
				[=](StorageSystemHolder const& C)
				{
					return C.layer > layer;
				}
			);

			auto span = std::span( find, find2 );

			std::size_t tar_index = std::distance(graphic_node.begin(), find2);
			std::size_t i = std::distance(graphic_node.begin(), find);

			for (auto& ite : span)
			{
				auto ite_pro = ite.GetProperty();
				if (!ite_pro.IsSameSystem(property))
				{
					auto ite_mutex = ite.GetMutex();
					if (ite_mutex.IsConflict(mutex))
					{
						auto K = priority.CompareCustomPriority(
							property, ite.priority, ite_pro
						);

						if (K == std::partial_ordering::greater)
						{
							dep.push_back({
								false, tar_index, i
								});
							has_to_node = true;
						}
						else if (K == std::partial_ordering::less)
						{
							dep.push_back({
								false, i, tar_index
								});
							in_degree += 1;
						}
						else if (K == std::partial_ordering::equivalent)
						{
							dep.push_back({
								true, i, tar_index
								});
							dep.push_back({
								true, tar_index, i
								});
						}
						else
						{
							return {
								SystemRegisterResult::Status::ConfuseDependence
							};
						}
					}
				}
				else
				{
					return {
						SystemRegisterResult::Status::ExistName
					};
				}
				++i;
			}

			if (has_to_node && in_degree != 0)
			{

				std::vector<std::size_t> search_stack;
				std::set<std::size_t> exist;
				exist.insert(tar_index);

				for (auto& ite : dep)
				{
					if (!ite.is_mutex && ite.from == tar_index)
					{
						search_stack.push_back(ite.to);
						exist.insert(ite.to);
					}
				}

				while(!search_stack.empty())
				{
					auto top = *search_stack.rbegin();
					search_stack.pop_back();

					for(auto& ite : graphic_node[top].trigger_to)
					{
						if(!ite.is_mutex)
						{
							auto [rite, suc] = exist.insert(ite.to);
							if(suc)
							{
								return {
									SystemRegisterResult::Status::CircleDependence,
									tar_index,
									in_degree
								};
							}else
							{
								search_stack.push_back(ite.to);
							}
						}
					}
				}
			}

			return {
				SystemRegisterResult::Status::Available,
				tar_index,
				in_degree
			};
		}
		else
		{
			return {
				SystemRegisterResult::Status::EmptyName
			};
		}
	}

	void TickSystemsGroup::Register(std::int32_t layer, SystemProperty property, SystemPriority priority, ArchetypeComponentManager& manager,
		FilterGenerator const& generator, SystemRegisterResult const& result,
		std::pmr::vector<SystemTemporaryDependenceLine> const& d_line, SystemHolder::Ptr ptr,
		std::u8string_view display_name_proxy, std::pmr::memory_resource* resource)
	{
		assert(ptr);
		for(auto& ite : generator.need_register_component)
		{
			manager.RegisterComponentFilter(ite.GetPointer(), reinterpret_cast<std::size_t>(ptr.GetPointer()));
		}

		auto insert_ite = graphic_node.begin() + result.ite_index;

		for (auto ite = insert_ite; ite != graphic_node.end(); ++ite)
		{
			auto old_span = std::span(ite->trigger_to);
			for (auto& ite : old_span)
			{
				if (ite.to >= result.ite_index)
				{
					ite.to += 1;
				}
			}
		}

		std::pmr::u8string total_string(resource);

		Potato::Format::FormatWritter<char8_t> format_writer;

		static Potato::Format::StaticFormatPattern<u8"{}{}{}-[{}]:[{}]"> for_pattern;

		for_pattern.Format(format_writer, property.group_name, property.system_name, display_name_proxy, property.group_name, property.system_name);

		total_string.resize(
			format_writer.GetWritedSize()
		);

		Potato::Format::FormatWritter<char8_t> format_writer2{ total_string };

		for_pattern.Format(format_writer2, property.group_name, property.system_name, display_name_proxy, property.group_name, property.system_name);

		auto mutex = generator.GetMutex();

		std::pmr::vector<SystemRWInfo> infos(resource);

		{
			
			infos.reserve(
				mutex.component_rw_infos.size()
			);

			infos.insert(
				infos.end(),
				mutex.component_rw_infos.begin(),
				mutex.component_rw_infos.end()
			);

		}

		graphic_node.emplace(
			insert_ite,
			layer,
			priority,
			std::move(total_string),
			Potato::Misc::IndexSpan<>{ 0, property.group_name.size() },
			Potato::Misc::IndexSpan<>{ property.group_name.size(), property.group_name.size() + property.system_name.size() },
			Potato::Misc::IndexSpan<>{ property.group_name.size() + property.system_name.size(), format_writer.GetWritedSize() },
			std::move(infos),
			Potato::Misc::IndexSpan<>{ 0, mutex.component_rw_infos.size() },
			std::pmr::vector<TriggerTo>{resource},
			result.in_degree,
			std::move(ptr)
		);

		for (auto& ite : d_line)
		{
			graphic_node[ite.from].trigger_to.emplace_back(
				ite.is_mutex,
				ite.to
			);
			if(!ite.is_mutex)
			{
				graphic_node[ite.to].in_degree += 1;
			}
		}

		need_refresh_dependence = true;
	}


	void TickSystemsGroup::FlushAndInitRegisterSystem(ArchetypeComponentManager& manager)
	{
		startup_system_context_ite = 0;
		current_level_system_waiting = 0;
		if (need_refresh_dependence)
		{
			need_refresh_dependence = false;

			system_contexts.clear();
			startup_system.clear();
			tick_systems_running_graphic_line.clear();

			if (!need_destroy_graphic.empty())
			{
				for (auto ite : need_destroy_graphic)
				{
					assert(ite < graphic_node.size());
					auto& tar = graphic_node[ite];
					auto start = std::find_if(graphic_node.begin(), graphic_node.end(), 
					[&](StorageSystemHolder& gn) { return gn.layer >= tar.layer; }
					);
					assert(start <= graphic_node.begin() + ite);
					for (auto ite2 = start; ite2 < graphic_node.end(); ++ite2)
					{
						if (ite2->layer == tar.layer)
						{
							for (auto& ite3 : ite2->trigger_to)
							{
								std::erase_if(
									ite2->trigger_to, [=](TriggerTo const& to) { return to.to == ite; }
								);
							}
						}
						else
						{
							for (auto& ite3 : ite2->trigger_to)
							{
								if (ite3.to > ite)
									ite3.to -= 1;
							}
						}
					}
					std::size_t unique_code = reinterpret_cast<std::size_t>(tar.system_obj.GetPointer());
					manager.ErasesComponentFilter(unique_code);
					graphic_node.erase(graphic_node.begin() + ite);
				}
				need_destroy_graphic.clear();
			}

			
			std::size_t index = 0;
			for (auto& ite : graphic_node)
			{
				auto& cur = *ite.system_obj;

				std::size_t o_size = tick_systems_running_graphic_line.size();
				for (auto& ite2 : ite.trigger_to)
				{
					auto& toref = graphic_node[ite2.to];
					tick_systems_running_graphic_line.emplace_back(
						ite2.is_mutex,
						ite2.to,
						toref.display_name.Slice(std::u8string_view(toref.total_string))
					);
				}

				system_contexts.emplace_back(
					RunningStatus::Ready,
					ite.in_degree,
					ite.in_degree,
					0,
					Potato::Misc::IndexSpan<>{o_size, tick_systems_running_graphic_line.size() },
					ite.system_obj
				);

				if(ite.in_degree == 0)
				{
					startup_system.emplace_back(
						ite.layer,
						index,
						ite.display_name.Slice(std::u8string_view(ite.total_string))
					);
				}
				++index;
			}

			return;
		}

		for (auto& ite : system_contexts)
		{
			ite.status = RunningStatus::Ready;
			ite.current_in_degree = ite.startup_in_degree;
			ite.mutex_degree = 0;
		}
		
	}

	std::size_t TickSystemsGroup::SynFlushAndDispatchImp(ArchetypeComponentManager& manager, void(*func)(void* obj, std::size_t, std::u8string_view), void* data)
	{
		std::lock_guard lg(graphic_mutex);
		std::lock_guard lg2(tick_system_running_mutex);
		FlushAndInitRegisterSystem(manager);
		if (!startup_system.empty())
		{
			std::size_t count = 0;
			std::optional<std::size_t> cache_index;
			for (auto& ite : startup_system)
			{
				if (!cache_index.has_value())
				{
					cache_index = ite.layer;
				}
				if (*cache_index == ite.layer)
				{
					++startup_system_context_ite;
					++current_level_system_waiting;
					++count;
					DispatchSystemImp(ite.index);
					func(data, ite.index, ite.display_name);
				}
				else
				{
					return count;
				}
			}
			return count;
		}
		return 0;
	}

	void TickSystemsGroup::DispatchSystemImp(std::size_t index)
	{
		auto& ref = system_contexts[index];
		ref.status = RunningStatus::Running;
		auto span = ref.reference_trigger_line.Slice(std::span(tick_systems_running_graphic_line));
		for (auto ite : span)
		{
			auto& ref2 = system_contexts[ite.index];
			if (ite.is_mutex)
			{
				ref2.mutex_degree += 1;
			}
		}
	}

	std::optional<std::size_t> TickSystemsGroup::TryDispatchDependenceImp(std::size_t index, void(*func)(void* obj, std::size_t, std::u8string_view), void* data)
	{
		if (tick_system_running_mutex.try_lock())
		{
			std::size_t count = 0;
			std::lock_guard lg2(tick_system_running_mutex, std::adopt_lock);
			assert(index < system_contexts.size());
			auto& ref = system_contexts[index];
			assert(ref.status == RunningStatus::Running);
			ref.status = RunningStatus::Done;

			auto span = ref.reference_trigger_line.Slice(std::span(tick_systems_running_graphic_line));
			for (auto ite : span)
			{
				auto& ref2 = system_contexts[ite.index];
				if (ite.is_mutex)
				{
					ref2.mutex_degree -= 1;
				}else
				{
					ref2.current_in_degree -= 1;
				}
				if(ref2.status == RunningStatus::Ready && ref2.current_in_degree == 0 && ref2.mutex_degree == 0)
				{
					++current_level_system_waiting;
					++count;
					DispatchSystemImp(ite.index);
					func(data, ite.index, ite.display_name);
				}
			}


			--current_level_system_waiting;


			if (current_level_system_waiting == 0)
			{
				std::optional<std::size_t> cache_layer;
				while (startup_system_context_ite < startup_system.size())
				{
					auto& ref = startup_system[startup_system_context_ite];
					if(!cache_layer.has_value())
						cache_layer = ref.layer;
					if (*cache_layer == ref.layer)
					{
						++startup_system_context_ite;
						++current_level_system_waiting;
						++count;
						DispatchSystemImp(ref.index);
						func(data, ref.index, ref.display_name);
					}else
					{
						break;
					}
				}
			}
			return current_level_system_waiting;
		}
		else
		{
			return std::nullopt;
		}
	}

	void TickSystemsGroup::ExecuteSystem(std::size_t index, ArchetypeComponentManager& manager, Context& context)
	{
		SystemHolder::Ptr holder;
		std::int32_t layer;
		SystemProperty pro;
		{
			std::shared_lock sl(graphic_mutex);
			assert(index < graphic_node.size());
			auto& ptr = graphic_node[index];
			holder = ptr.system_obj;
			layer = ptr.layer;
			pro = ptr.GetProperty();
		}
		assert(holder);
		holder->Execute(manager, context, layer, pro);

	}
	
}