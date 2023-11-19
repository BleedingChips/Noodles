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

	bool SystemContext::StartParallel(std::size_t parallel_count)
	{
		return system_group.StartParallel(ptr, parallel_count);
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


	std::tuple<void const*, std::size_t> SystemComponentFilter::Wrapper::ReadRaw(UniqueTypeID const& ref, std::size_t index) const
	{
		assert(index < id_index.size());
		auto idi = id_index[index];
		if(ref == infos[index].type_id)
		{
			return { archetype.GetData(idi.index, 0, mp), idi.count };
		}
		return {nullptr, 0};
	}

	std::tuple<void*, std::size_t> SystemComponentFilter::Wrapper::WriteRaw(UniqueTypeID const& ref, std::size_t index) const
	{
		assert(index < id_index.size());
		auto idi = id_index[index];
		if (ref == infos[index].type_id && infos[index].is_write)
		{
			return { archetype.GetData(idi.index, 0, mp), idi.count };
		}
		return {nullptr, 0};
	}

	std::tuple<void const*, std::size_t> SystemEntityFilter::Wrapper::ReadRaw(UniqueTypeID const& ref, std::size_t index) const
	{
		assert(index < location.size());
		auto idi = location[index];
		if (ref == infos[index].type_id)
		{
			return { archetype.GetData(idi.index, 0, mp), idi.count };
		}
		return { nullptr, 0 };
	}

	std::tuple<void*, std::size_t> SystemEntityFilter::Wrapper::WriteRaw(UniqueTypeID const& ref, std::size_t index) const
	{
		assert(index < location.size());
		auto idi = location[index];
		if (ref == infos[index].type_id && infos[index].is_write)
		{
			return { archetype.GetData(idi.index, 0, mp), idi.count };
		}
		return { nullptr, 0 };
	}

	static Potato::Format::StaticFormatPattern<u8"{}{}{}-[{}]:[{}]"> system_static_format_pattern;

	std::size_t SystemHolder::FormatDisplayNameSize(std::u8string_view prefix, SystemProperty property)
	{
		Potato::Format::FormatWritter<char8_t> wri;
		system_static_format_pattern.Format(wri, property.group_name, property.system_name, prefix, property.group_name, property.system_name);
		return wri.GetWritedSize();
	}

	bool SystemHolder::FormatDisplayName(std::span<char8_t> output, std::u8string_view prefix, SystemProperty property)
	{
		Potato::Format::FormatWritter<char8_t> wri(output);
		return system_static_format_pattern.Format(wri, property.group_name, property.system_name, prefix, property.group_name, property.system_name);
	}

	SystemHolder::SystemHolder(std::span<std::byte> output, std::u8string_view prefix, SystemProperty in_property)
	{
		std::span<char8_t> tem{reinterpret_cast<char8_t*>(output.data()), output.size() / sizeof(char8_t)};
		auto re = FormatDisplayName(tem, prefix, in_property);
		assert(re);
		std::u8string_view tstr{ tem };
		property.group_name = tstr.substr(0, in_property.group_name.size());
		property.system_name = tstr.substr(in_property.group_name.size(), in_property.system_name.size());
		display_name = tstr.substr(in_property.system_name.size() + in_property.group_name.size());
	}

	TickSystemsGroup::TickSystemsGroup(std::pmr::memory_resource* resource)
		: system_holder_resource(resource), graphic_node(resource),
		total_string(resource), total_rw_info(resource), need_destroy_graphic(resource),
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
				auto ite_pro = ite.GetProperty(total_string);
				if (!ite_pro.IsSameSystem(property))
				{
					auto ite_mutex = ite.GetMutex(total_rw_info);
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

		Potato::Misc::IndexSpan<> group_name{ total_string.size(), total_string.size() };

		total_string.reserve(
			group_name.End() + property.group_name.size() + property.system_name.size()
		);

		total_string.append(property.group_name);

		group_name.BackwardEnd(property.group_name.size());

		Potato::Misc::IndexSpan<> system_name{ total_string.size(), total_string.size() };

		total_string.append(property.system_name);

		system_name.BackwardEnd(property.system_name.size());

		Potato::Misc::IndexSpan<> rw_index{ total_rw_info.size(), total_rw_info.size() };

		total_rw_info.reserve(
			rw_index.End() + generator.component_rw_infos.size()
		);

		total_rw_info.insert(
			total_rw_info.end(),
			generator.component_rw_infos.begin(),
			generator.component_rw_infos.end()
		);

		rw_index.BackwardEnd(generator.component_rw_infos.size());
		
		/*
		Potato::Format::FormatWritter<char8_t> format_writer;

		system_static_format_pattern.Format(format_writer, property.group_name, property.system_name, display_name_proxy, property.group_name, property.system_name);

		Potato::Format::FormatWritter<char8_t> format_writer2{ total_string };
		*/

		//for_pattern.Format(format_writer2, property.group_name, property.system_name, display_name_proxy, property.group_name, property.system_name);

		std::pmr::vector<TriggerTo> tris(resource);

		for (auto& ite : d_line)
		{
			if(ite.from == result.ite_index)
			{
				tris.push_back({ite.is_mutex, ite.to});
				if(!ite.is_mutex)
				{
					graphic_node[ite.to].in_degree += 1;
				}
			}
		}

		for(auto& ite : d_line)
		{
			if(ite.from != result.ite_index)
			{
				graphic_node[ite.from].trigger_to.emplace_back(
					ite.is_mutex,
					ite.to
				);
			}
		}

		graphic_node.emplace(
			insert_ite,
			layer,
			priority,
			group_name,
			system_name,
			rw_index,
			std::move(tris),
			result.in_degree,
			std::move(ptr)
		);

		need_refresh_dependence = true;
	}

	bool TickSystemsGroup::StartupNewLayerSystems(void(*func)(void* obj, TickSystemRunningIndex index, std::u8string_view), void* data)
	{
		auto span = std::span(startup_system).subspan(startup_system_context_ite);
		std::optional<std::size_t> cache_index;
		for(auto& ite : span)
		{
			if (!cache_index.has_value())
			{
				cache_index = ite.layer;
			}
			if (*cache_index == ite.layer)
			{
				++startup_system_context_ite;
				++current_level_system_waiting;
				{
					DispatchSystemImp(*ite.to);
				}
				
				ite.to->AddRef();
				func(data, { reinterpret_cast<std::size_t>(ite.to.GetPointer()), 0 }, ite.display_name);
			}
			else
			{
				return true;
			}
		}
		return cache_index.has_value();
	}

	bool TickSystemsGroup::SynFlushAndDispatchImp(ArchetypeComponentManager& manager, void(*func)(void* obj, TickSystemRunningIndex index, std::u8string_view), void* data)
	{
		std::lock_guard lg(tick_system_running_mutex);
		startup_system_context_ite = 0;
		current_level_system_waiting = 0;

		{
			std::lock_guard lg(graphic_mutex);
			if (need_refresh_dependence)
			{
				need_refresh_dependence = false;

				startup_system.clear();
				tick_systems_running_graphic_line.clear();
				temporary_context.clear();
				running_context.clear();

				if (!need_destroy_graphic.empty())
				{
					for (auto ite : need_destroy_graphic)
					{
						assert(ite < graphic_node.size());
						auto& tar = graphic_node[ite];

						total_string.erase(
							total_string.begin() + tar.group_name.Begin(),
							total_string.begin() + tar.system_name.End()
						);

						std::size_t str_size = tar.group_name.Size() + tar.system_name.Size();

						total_rw_info.erase(
							total_rw_info.begin() + tar.component_filter.Begin(),
							total_rw_info.begin() + tar.component_filter.End()
						);

						std::size_t rw_size = tar.component_filter.Size();

						for (auto ite2 = graphic_node.begin() + ite + 1; ite2 != graphic_node.end(); ++ite2)
						{
							if (ite2->group_name.Begin() >= tar.system_name.End())
							{
								ite2->group_name = {
									ite2->group_name.Begin() - str_size,
									ite2->group_name.End() - str_size,
								};

								ite2->system_name = {
									ite2->system_name.Begin() - str_size,
									ite2->system_name.End() - str_size,
								};
							}

							if (ite2->component_filter.Begin() >= tar.component_filter.End())
							{
								ite2->component_filter = {
									ite2->component_filter.Begin() - rw_size,
									ite2->component_filter.End() - rw_size,
								};
							}
						}

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

					std::lock_guard lg(cur.mutex);

					std::size_t o_size = tick_systems_running_graphic_line.size();
					for (auto& ite2 : ite.trigger_to)
					{
						auto& toref = graphic_node[ite2.to];
						tick_systems_running_graphic_line.emplace_back(
							ite2.is_mutex,
							ite2.to,
							toref.system_obj,
							toref.system_obj->GetDisplayName()
						);
					}

					cur.status = RunningStatus::Ready;
					cur.request_parallel = 0;

					cur.fast_index = running_context.size();

					running_context.emplace_back(
						ite.in_degree,
						ite.in_degree,
						0,
						Potato::Misc::IndexSpan<>{ o_size, tick_systems_running_graphic_line.size()},
						ite.system_obj
					);

					if (ite.in_degree == 0)
					{
						startup_system.emplace_back(
							ite.layer,
							ite.system_obj,
							ite.system_obj->GetDisplayName()
						);
					}
					++index;
				}
			}
		}
		

		for (auto& ite : graphic_node)
		{
			auto& ref = *ite.system_obj;
			std::lock_guard lf(ref.mutex);
			ref.status = RunningStatus::Ready;
			ref.request_parallel = 0;
		}

		for (auto& ite : running_context)
		{
			ite.mutex_degree = 0;
			ite.current_in_degree = ite.startup_in_degree;
		}
		
		if (!startup_system.empty())
		{
			return StartupNewLayerSystems(func, data);
		}
		return false;
	}

	void TickSystemsGroup::DispatchSystemImp(SystemHolder& ref)
	{
		{
			std::lock_guard lg(ref.mutex);
			assert(ref.status == RunningStatus::Ready);
			ref.status = RunningStatus::Waiting;
		}

		auto& ref2 = running_context[ref.fast_index];
		auto span = ref2.reference_trigger_line.Slice(std::span(tick_systems_running_graphic_line));

		for(auto& ite : span)
		{
			if (ite.is_mutex)
			{
				running_context[ite.to->fast_index].mutex_degree += 1;
			}
		}
	}

	bool TickSystemsGroup::StartParallel(SystemHolder& system, std::size_t parallel_count)
	{
		std::lock_guard lg(system.mutex);
		if(system.status == RunningStatus::Running && system.request_parallel == 0)
		{
			system.request_parallel = parallel_count;
			return true;
		}
		return false;
	}

	bool TickSystemsGroup::ExecuteAndDispatchDependence(TickSystemRunningIndex index, ArchetypeComponentManager& manager, Context& context, void(*func)(void* obj, TickSystemRunningIndex, std::u8string_view), void* data)
	{

		SystemHolder::Ptr ptr{ reinterpret_cast<SystemHolder*>(index.index) };
		

		SystemContext sys_context{
			*ptr, manager, context, *this
		};

		sys_context.self_property = ptr->GetProperty();

		{
			if(ptr->mutex.try_lock())
			{
				std::lock_guard lg(ptr->mutex, std::adopt_lock);
				switch (ptr->status)
				{
				case RunningStatus::Waiting:
					ptr->status = RunningStatus::Running;
					break;
				case RunningStatus::WaitingParallel:
					if(ptr->request_parallel == 0)
					{
						sys_context.category = SystemCatergory::FinalParallel;
						ptr->status = RunningStatus::Running;
					}else
					{
						sys_context.category = SystemCatergory::Parallel;
						sys_context.parameter = index.parameter;
					}
					break;
				default:
					assert(false);
					break;
				}
			}else
			{
				//ptr->AddRef();
				func(data, index, ptr->GetDisplayName());
				return true;
			}
		}

		ptr->SubRef();

		ptr->Execute(sys_context);

		RunningStatus status = RunningStatus::Done;
		

		{
			std::lock_guard lg2(ptr->mutex);

			switch(ptr->status)
			{
			case RunningStatus::Running:
				if(ptr->request_parallel == 0)
				{
					ptr->status = RunningStatus::Done;
					break;
				}else
				{
					ptr->status = RunningStatus::WaitingParallel;
					status = RunningStatus::WaitingParallel;
					for(std::size_t i = 0; i < ptr->request_parallel; ++i)
					{
						ptr->AddRef();
						func(data, { index.index, i}, ptr->GetDisplayName());
					}
					return true;
				}
				break;
			case RunningStatus::WaitingParallel:
				status = RunningStatus::WaitingParallel;
				--ptr->request_parallel;
				if(ptr->request_parallel == 0)
				{
					ptr->AddRef();
					func(data, { index.index, 0 }, ptr->GetDisplayName());
				}
				return true;
				break;
			default:
				assert(false);
				break;
			}
		}

		if(status == RunningStatus::Done)
		{
			{
				std::lock_guard lg(tick_system_running_mutex);

				auto index = ptr->fast_index;
				auto& ref = running_context[index];

				auto span = ref.reference_trigger_line.Slice(std::span(tick_systems_running_graphic_line));

				for (auto& ite : span)
				{
					{
						auto& tar = running_context[ite.to_index];
						if (ite.is_mutex)
						{
							assert(tar.mutex_degree > 0);
							tar.mutex_degree -= 1;
						}
						else {
							assert(tar.current_in_degree > 0);
							tar.current_in_degree -= 1;
						}

						{

							if(ref.mutex_degree == 0 && ref.current_in_degree == 0)
							{
								auto& ref2 = *tar.to;
								std::lock_guard lg(ref2.mutex);
								if(ref2.status == RunningStatus::Ready)
								{
									ref2.status = RunningStatus::Waiting;

									auto span2 = tar.reference_trigger_line.Slice(std::span(tick_systems_running_graphic_line));

									for (auto& ite : span)
									{
										if (ite.is_mutex)
										{
											running_context[ite.to_index].mutex_degree += 1;
										}
									}
								}
								ref2.AddRef();
								func(data, {reinterpret_cast<std::size_t>(tar.to.GetPointer()), 0}, ref2.GetDisplayName());
								++current_level_system_waiting;
							}
						}
					}
				}

				--current_level_system_waiting;
				if(current_level_system_waiting == 0)
				{
					StartupNewLayerSystems(func, data);
				}
				return current_level_system_waiting != 0;
			}
		}else
		{
			assert(false);
			{
				std::lock_guard lg(ptr->mutex);
				assert(ptr->status == RunningStatus::WaitingParallel);
				ptr->request_parallel -= 0;
				if(ptr->request_parallel == 0)
				{
					ptr->AddRef();
					func(data, {reinterpret_cast<std::size_t>(ptr.GetPointer()), 0}, ptr->GetDisplayName());
				}
				return true;
			}
		}
	}
	
}