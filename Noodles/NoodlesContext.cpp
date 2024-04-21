module;

#include <cassert>

module NoodlesContext;
import PotatoFormat;

namespace Noodles
{

	bool DetectConflict(std::span<RWUniqueTypeID const> t1, std::span<RWUniqueTypeID const> t2)
	{
		auto ite1 = t1.begin();
		auto ite2 = t2.begin();

		while (ite1 != t1.end() && ite2 != t2.end())
		{
			auto re = ite1->type_id <=> ite2->type_id;
			if (re == std::strong_ordering::equal)
			{
				if (
					ite1->is_write
					|| ite2->is_write
					)
				{
					return true;
				}
				else
				{
					++ite2; ++ite1;
				}
			}
			else if (re == std::strong_ordering::less)
			{
				++ite1;
			}
			else
			{
				++ite2;
			}
		}
		return false;
	}

	bool ReadWriteMutex::IsConflict(ReadWriteMutex const& mutex) const
	{
		return DetectConflict(components, mutex.components) || DetectConflict(singleton, mutex.singleton);
	}

	void ReadWriteMutexGenerator::RegisterComponentMutex(std::span<RWUniqueTypeID const> ifs)
	{
		for(auto& ite : ifs)
		{
			if (ite.ignore_mutex)
				continue;
			auto spn = std::span(unique_ids).subspan(0, component_count);
			bool Find = false;
			for(auto& ite2 : spn)
			{
				if(ite.type_id == ite2.type_id)
				{
					ite2.is_write = ite2.is_write || ite.is_write;
					Find = true;
					break;
				}
			}
			if(!Find)
			{
				unique_ids.insert(unique_ids.begin() + component_count, ite);
				++component_count;
			}
		}
	}

	void ReadWriteMutexGenerator::RegisterSingletonMutex(std::span<RWUniqueTypeID const> ifs)
	{
		for (auto& ite : ifs)
		{
			if (ite.ignore_mutex)
				continue;
			auto spn = std::span(unique_ids).subspan(component_count);
			bool Find = false;
			for (auto& ite2 : spn)
			{
				if (ite.type_id == ite2.type_id)
				{
					ite2.is_write = ite2.is_write || ite.is_write;
					Find = true;
					break;
				}
			}
			if (!Find)
			{
				unique_ids.emplace_back(ite);
			}
		}
	}

	ReadWriteMutex ReadWriteMutexGenerator::GetMutex() const
	{
		return ReadWriteMutex{
			std::span(unique_ids).subspan(0, component_count),
			std::span(unique_ids).subspan(component_count),
			system_id
		};
	}

	static Potato::Format::StaticFormatPattern<u8"{}{}{}-[{}]:[{}]"> system_static_format_pattern;

	std::size_t SystemHolder::FormatDisplayNameSize(std::u8string_view prefix, Property property)
	{
		Potato::Format::FormatWritter<char8_t> wri;
		system_static_format_pattern.Format(wri, property.group, property.name, prefix, property.group, property.name);
		return wri.GetWritedSize();
	}

	std::optional<std::tuple<std::u8string_view, Property>> SystemHolder::FormatDisplayName(std::span<char8_t> output, std::u8string_view prefix, Property property)
	{
		Potato::Format::FormatWritter<char8_t> wri(output);
		auto re = system_static_format_pattern.Format(wri, property.group, property.name, prefix, property.group, property.name);
		if(re)
		{
			return std::tuple<std::u8string_view, Property>{
				std::u8string_view{output.data(), wri.GetWritedSize()}.substr(property.group.size() + property.name.size()),
				Property{
					std::u8string_view{output.data() + property.group.size(), property.name.size()},
					std::u8string_view{output.data(), property.group.size()},
				}
			};
		}
		return std::nullopt;
	}

	void SystemHolder::TaskFlowNodeExecute(Potato::Task::TaskFlowStatus& status)
	{
		ExecuteContext context
		{
			status.context,
			static_cast<Context&>(status.owner)
		};
		SystemExecute(context);
	}

	Context::Context(Config config, std::u8string_view name, SyncResource resource) noexcept
		: config(config), name(name), 
		manager({
			resource.context_resource,
			resource.archetype_resource,
			resource.component_resource,
			resource.singleton_resource,
			resource.temporary_resource
		}),
		systems(resource.context_resource),
		rw_unique_id(resource.context_resource),
		system_resource(resource.system_resource),
		entity_resource(resource.entity_resource),
		temporary_resource(resource.temporary_resource),
		context_resource(resource.context_resource)
	{

	}

	bool Context::FlushSystemStatus(std::pmr::vector<Potato::Task::TaskFlow::ErrorNode>* error)
	{
		bool taskflow_need_update = false;

		{
			std::lock_guard lg(mutex);
			if(need_update)
			{
				taskflow_need_update = true;
			}
		}

		{
			std::lock_guard lg(system_mutex);
			if(system_need_remove)
			{
				for(auto & ite : systems)
				{
					if(ite.status == SystemStatus::NeedRemove)
					{
						TaskFlow::Remove(ite.system.GetPointer());
						taskflow_need_update = true;
					}
				}
			}
		}

		if(taskflow_need_update && TaskFlow::TryUpdate(error, context_resource))
		{
			{
				std::lock_guard lg(mutex);
				if(need_update)
				{
					need_update = false;
				}
			}

			{
				std::lock_guard lg(system_mutex);
				if(system_need_remove)
				{
					system_need_remove = false;
					std::size_t offset = 0;
					for(auto & ite : systems)
					{
						ite.component_index.WholeForward(offset);
						ite.singleton_index.WholeForward(offset);
						if(ite.status == SystemStatus::NeedRemove)
						{
							auto start = ite.component_index.Begin();
							auto end = ite.singleton_index.End();
							rw_unique_id.erase(rw_unique_id.begin() + start, rw_unique_id.begin() + end);
							offset += end - start;
							UnRegisterFilter(*ite.system);
							TaskFlow::Remove(ite.system.GetPointer());
						}
					}

					systems.erase(std::remove_if(systems.begin(), systems.end(), [](SystemTuple const& tup)
					{
						return tup.status == SystemStatus::NeedRemove;
					}), systems.end());
				}
			}
			return true;
		}


		
		return false;
	}

	void Context::FlushStats()
	{
		FlushSystemStatus();
		manager.ForceUpdateState();
	}

	bool Context::RemoveSystemDefer(Property require_property)
	{
		std::lock_guard lg(system_mutex);
		std::size_t count = 0;
		for(auto& ite : systems)
		{
			assert(ite.system);
			if(ite.status == SystemStatus::Normal && ite.system->GetProperty() == require_property)
			{
				ite.status = SystemStatus::NeedRemove;
				count += 1;
				system_need_remove = true;
			}
		}
		return count;
	}

	bool Context::RemoveSystemDeferByGroud(std::u8string_view group_name)
	{
		std::lock_guard lg(system_mutex);
		std::size_t count = 0;
		for(auto& ite : systems)
		{
			assert(ite.system);
			if(ite.status == SystemStatus::Normal && ite.system->GetProperty().group == group_name)
			{
				ite.status = SystemStatus::NeedRemove;
				count += 1;
				need_update = true;
			}
		}
		return count;
	}

	bool Context::Commit(Potato::Task::TaskContext& context, Potato::Task::TaskFilter task_filter, Potato::Task::AppendData user_data)
	{
		Potato::Task::TaskProperty pro
		{
			name,
			user_data,
			task_filter
		};
		return TaskFlow::Commit(context, pro, context_resource);
	}

	void Context::TaskFlowExecuteBegin(Potato::Task::ExecuteStatus& status, Potato::Task::TaskFlowExecute& execute)
	{
		std::lock_guard lg(mutex);
		start_up_tick_lock = std::chrono::steady_clock::now();
		std::println("---start");
	}

	void Context::TaskFlowExecuteEnd(Potato::Task::ExecuteStatus& status, Potato::Task::TaskFlowExecute& execute)
	{
		std::chrono::steady_clock::time_point target_time = std::chrono::steady_clock::now();
		{
			std::lock_guard lg(mutex);
			std::println("---finish");
			if (require_quit)
			{
				require_quit = false;
				return;
			}
			target_time = start_up_tick_lock + config.min_frame_time;
		}


		bool need_refresh = FlushSystemStatus();
		manager.ForceUpdateState();
		if(need_refresh)
		{
			if(execute.ReCloneNode())
			{
				execute.Reset();
				execute.Commit(status.context,target_time);
			}else
			{
				TaskFlow::Commit(status.context, status.task_property, context_resource);
			}
		}else
		{
			execute.Reset();
			execute.Commit(status.context,target_time);
		}
	}

	bool Context::RegisterSystem(SystemHolder::Ptr ptr, Priority priority, Property property, OrderFunction func, std::optional<Potato::Task::TaskFilter> task_filter, ReadWriteMutexGenerator& generator)
	{
		bool need_update = false;
		if(ptr)
		{
			std::lock_guard lg(system_mutex);

			Potato::Task::TaskFlowNode::Ptr tptr = ptr.GetPointer();

			struct Potato::Task::NodeProperty np
			{
				ptr->GetDisplayName(),
				task_filter
			};

			if(AddNode(tptr, np))
			{
				auto mutex = generator.GetMutex();

				for(auto& ite : systems)
				{
					if(ite.status == SystemStatus::NeedRemove)
						continue;
					auto re = ite.priority <=> priority;
					bool is_conflict = false;

					if (ite.priority.layout != priority.layout)
					{
						is_conflict = true;
					}
					else 
					{
						ReadWriteMutex ite_mutex{
						ite.component_index.Slice(std::span(rw_unique_id)),
						ite.singleton_index.Slice(std::span(rw_unique_id)),
							generator.system_id
						};
						is_conflict = ite_mutex.IsConflict(mutex);
						if(is_conflict && re == std::strong_ordering::equal)
						{
							auto p1 = (ite.order_function == nullptr) ?
								Order::UNDEFINE : ite.order_function(ite.property, property);
							auto p2 = (func == nullptr) ?
								Order::UNDEFINE : func(ite.property, property);
							auto p3 = Order::UNDEFINE;
							if (p1 == p2)
								p3 = p1;
							else if (p1 == Order::UNDEFINE || p1 == Order::MUTEX)
								p3 = p2;
							else if (p2 == Order::UNDEFINE || p2 == Order::MUTEX)
								p3 = p1;
							else
							{
								Remove(tptr);
								return false;
							}
							if (p3 == Order::SMALLER)
								re = std::strong_ordering::less;
							else if(p3 == Order::BIGGER)
								re = std::strong_ordering::greater;
						}
					}

					if(is_conflict)
					{
						Potato::Task::TaskFlowNode::Ptr tptr2{ite.system.GetPointer()};
						if (re == std::strong_ordering::less)
							AddDirectEdges(std::move(tptr2), tptr);
						else if (re == std::strong_ordering::greater)
							AddDirectEdges(tptr, std::move(tptr2));
						else
							AddMutexEdges(tptr, std::move(tptr2));
					}
				}

				auto osize = rw_unique_id.size();
				rw_unique_id.insert(rw_unique_id.end(), mutex.components.begin(), mutex.components.end());
				Potato::Misc::IndexSpan<> comp_ind{osize, rw_unique_id.size()};
				osize = rw_unique_id.size();
				rw_unique_id.insert(rw_unique_id.end(), mutex.singleton.begin(), mutex.singleton.end());
				Potato::Misc::IndexSpan<> single_ind{ osize, rw_unique_id.size() };

				systems.emplace_back(
					ptr,
					property,
					priority,
					comp_ind,
					single_ind,
					mutex.system,
					func,
					SystemStatus::Normal
				);
				need_update = true;
			}
		}
		if(need_update)
		{
			ptr->SystemInit(*this);
			return true;
		}
		return false;
	}
}