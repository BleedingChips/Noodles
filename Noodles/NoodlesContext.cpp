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

	void Context::AddTaskRef() const
	{
		DefaultIntrusiveInterface::AddRef();
	}

	void Context::SubTaskRef() const
	{
		DefaultIntrusiveInterface::SubRef();
	}

	void Context::Release()
	{
		auto re = record;
		this->~Context();
		re.Deallocate();
	}

	Context::Context(Config config, std::u8string_view name, Potato::IR::MemoryResourceRecord record, SyncResource resource) noexcept
		: config(config), name(name), record(record), manager({resource.context_resource, resource.archetype_resource, resource.component_resource, resource.singleton_resource}),
		systems(resource.context_resource), rw_unique_id(resource.context_resource), system_resource(resource.system_resource)
	{

	}

	auto Context::Create(Config config, std::u8string_view name, SyncResource resource) -> Ptr
	{
		auto fix_layout = Potato::IR::Layout::Get<Context>();
		std::size_t offset = 0;
		if(!name.empty())
		{
			offset = Potato::IR::InsertLayoutCPP(fix_layout, Potato::IR::Layout::GetArray<char8_t>(name.size()));
			Potato::IR::FixLayoutCPP(fix_layout);
		}

		auto re = Potato::IR::MemoryResourceRecord::Allocate(resource.context_resource, fix_layout);
		if(re)
		{
			if(!name.empty())
			{
				std::memcpy(re.GetByte() + offset, name.data(), sizeof(char8_t) * name.size());
				name = std::u8string_view{ reinterpret_cast<char8_t const*>(re.GetByte() + offset), name.size() };
			}
			
			return new (re.Get()) Context{config, name, re, resource };
		}
		return {};
	}

	void Context::FlushStats()
	{
		TaskFlow::Update(true);
		manager.ForceUpdateState();
	}

	bool Context::Commit(Potato::Task::TaskContext& context, Potato::Task::TaskProperty property)
	{
		return TaskFlow::Commit(context, property, name);
	}

	void Context::OnBeginTaskFlow(Potato::Task::ExecuteStatus& status)
	{
		std::lock_guard lg(mutex);
		start_up_tick_lock = std::chrono::steady_clock::now();
		std::println("---start");
	}

	Context::ComponentArchetypeMountPointRange Context::IterateComponent(ComponentFilterInterface const& interface, std::size_t ite_index, std::span<std::size_t> output_span) const
	{
		auto [ar, mb, me, sp] = manager.ReadComponents(interface, ite_index, output_span);
		if(ar)
		{
			return ComponentArchetypeMountPointRange{std::move(ar), mb, me, sp};
		}
		return ComponentArchetypeMountPointRange{ {}, {}, {} };
	}

	Context::ComponentArchetypeMountPointRange Context::ReadEntity(Entity const& entity, ComponentFilterInterface const& interface, std::span<std::size_t> output_span) const
	{
		auto [ar, mp, sp] = manager.ReadEntity(entity, interface, output_span);
		if (ar)
		{
			auto end = mp;
			end += 1;
			return ComponentArchetypeMountPointRange{ std::move(ar), mp, end, sp };
		}
		return ComponentArchetypeMountPointRange{ {}, {}, {} };
	}

	void Context::OnFinishTaskFlow(Potato::Task::ExecuteStatus& status)
	{
		TaskFlow::Update(true);
		manager.ForceUpdateState();
		
		std::lock_guard lg(mutex);
		std::println("---finish");
		if (require_quit)
		{
			require_quit = false;
			return;
		}
		TaskFlow::CommitDelay(status.context, start_up_tick_lock + config.min_frame_time, status.task_property, name);
	}

	bool Context::RegisterSystem(SystemHolder::Ptr ptr, Priority priority, Property property, OrderFunction func, Potato::Task::TaskProperty task_property, ReadWriteMutexGenerator& generator)
	{
		bool need_update = false;
		if(ptr)
		{
			std::lock_guard lg(system_mutex);

			Potato::Task::TaskFlowNode::Ptr tptr = ptr.GetPointer();

			if(AddNode(tptr, task_property, ptr->GetDisplayName()))
			{
				auto mutex = generator.GetMutex();

				for(auto& ite : systems)
				{
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
					func
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