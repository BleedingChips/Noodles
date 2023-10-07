module;

#include <cassert>

module NoodlesContext;

namespace Noodles
{

	constexpr std::u8string_view TaskName = u8"Noodles Startup";

	auto Context::Create(Config config, Potato::Task::TaskContext::Ptr TaskPtr, std::pmr::memory_resource* UpstreamResource)->Ptr
	{
		if(TaskPtr && UpstreamResource != nullptr)
		{
			auto Adress = UpstreamResource->allocate(sizeof(Context), alignof(Context));
			if(Adress != nullptr)
			{
				return new (Adress) Context{ config, std::move(TaskPtr), UpstreamResource };
			}else
			{
				return {};
			}
		}
		return {};
	}

	Context::Context(Config config, Potato::Task::TaskContext::Ptr TaskPtr, std::pmr::memory_resource* Resource)
		: task_context(std::move(TaskPtr)), m_resource(Resource), /*
	EntityResource(Memory::HugePageMemoryResource::Create(Resource)),
	ArcheTypeResource(Resource),
	ComponentResource(Resource), */config(config)
	{
	}

	Context::~Context()
	{
		
	}

	void Context::operator()(Potato::Task::ExecuteStatus& status)
	{
		if(status.Property.AppendData == 0)
		{
			if(tick_system_mutex.try_lock())
			{
				std::lock_guard lg(tick_system_mutex, std::adopt_lock);
				FlushTickSystem();
			}

			if(tick_system_running_mutex.try_lock())
			{
				std::lock_guard lg(tick_system_running_mutex, std::adopt_lock);
				InitTickSystem();
				{
					std::lock_guard lg2(property_mutex);
					last_execute_time = std::chrono::system_clock::now();
				}
				TryFireTickSystem(status.Context, status.Property.AppendData);
			}else
			{
				status.Context.CommitTask(this, status.Property);
				return;
			}
		}else
		{
			std::size_t index = status.Property.AppendData - 1;
			System::Object::Ref ref;
			System::Property pro;
			if (tick_system_running_mutex.try_lock())
			{
				std::lock_guard lg(tick_system_running_mutex, std::adopt_lock);
				assert(tick_system_context.size() > index);
				auto& ite = tick_system_context[index];
				if (ite.status == SystemStatus::Waitting)
				{
					ref = ite.object;
					pro = ite.property;
					ite.status = SystemStatus::Running;
				}else if(ite.status == SystemStatus::Running)
				{
					ite.status = SystemStatus::Done;
					TryFireTickSystem(status.Context, status.Property.AppendData);
				}
			}else
			{
				status.Context.CommitTask(this, status.Property);
				return;
			}

			if(ref)
			{
				ExecuteContext Cont{
					pro,
					*this
				};
				ref.Execute(Cont);
				{
					if(tick_system_running_mutex.try_lock())
					{
						std::lock_guard lg(tick_system_running_mutex, std::adopt_lock);
						assert(tick_system_context.size() > index);
						tick_system_context[index].status = SystemStatus::Done;
						TryFireTickSystem(status.Context, status.Property.AppendData);
					}else
					{
						status.Context.CommitTask(this, status.Property);
						return;
					}
				}
			}
		}

		std::size_t E = 0;
		while (!running_task.compare_exchange_strong(
			E, E - 1
		))
		{

		}

		if (E == 2)
		{
			E = 1;
			auto re = running_task.compare_exchange_strong(E, 2);
			assert(re);
			std::chrono::system_clock::time_point require_time;
			Potato::Task::TaskContext::Ptr re_task_context;
			Potato::Task::TaskProperty new_pro;
			new_pro.TaskName = TaskName;
			{
				std::lock_guard lg(property_mutex);
				require_time = last_execute_time + config.min_frame_time;
				re_task_context = task_context;
				new_pro.TaskPriority = config.priority;
			}
			assert(re_task_context);
			re_task_context->CommitDelayTask(this, require_time, new_pro);
		}
	}

	void Context::TryFireTickSystem(Potato::Task::TaskContext& task_context, std::size_t index)
	{
		if(index == 0)
		{
			std::size_t i = 0;
			for(auto& ite : tick_system_context)
			{
				if(ite.layer == current_layer)
				{
					if(ite.status == SystemStatus::Ready && ite.in_degree == 0)
					{
						ite.status = SystemStatus::Waitting;
						Potato::Task::TaskProperty new_pro{
							ite.property.task_priority,
							ite.property.system_name,
					i + 1
						};
						std::size_t e = 0;
						while (!running_task.compare_exchange_strong(
							e, e + 1
						))
						{

						}
						task_context.CommitTask(this, new_pro);
					}
				}else
					break;
				++i;
			}
		}else
		{
			std::size_t real_index = index - 1;
			systems_count -= 1;
			if(systems_count == 0)
			{
				runs_system_count += current_layer_system_count;
				auto last_span = std::span(tick_system_context).subspan(runs_system_count);
				if(!last_span.empty())
				{
					current_layer = last_span[0].layer;
					systems_count = 0;
					std::size_t i = runs_system_count;
					for(auto& ite : last_span)
					{
						if(ite.layer == current_layer)
						{
							systems_count += 1;
							if(ite.in_degree == 0 && ite.status == SystemStatus::Ready)
							{
								ite.status = SystemStatus::Waitting;
								Potato::Task::TaskProperty new_pro{
									ite.property.task_priority,
									ite.property.system_name,
							i + 1
								};
								std::size_t e = 0;
								while (!running_task.compare_exchange_strong(
									e, e + 1
								))
								{

								}
								task_context.CommitTask(this, new_pro);
							}
						}
						i += 1;
					}
					current_layer_system_count = systems_count;
				}
			}else
			{
				auto& cur = tick_system_context[real_index];
				for(auto& ite : cur.graphic_line)
				{
					assert(ite.from_node == real_index);
					auto& tar = tick_system_context[ite.to_node];
					bool need_commit = true;
					for(auto& ite2 : tar.reverse_graphic_line)
					{
						assert(ite2.from_node == ite.to_node);
						auto status = tick_system_context[ite2.to_node].status;
						if(ite2.is_mutex && (status == SystemStatus::Running || status == SystemStatus::Waitting))
						{
							need_commit = false;
							break;
						}else if(status != SystemStatus::Done)
						{
							need_commit = false;
							break;
						}
					}
					if(need_commit)
					{
						tar.status = SystemStatus::Waitting;
						Potato::Task::TaskProperty new_pro{
							tar.property.task_priority,
							tar.property.system_name,
							ite.to_node + 1
						};
						std::size_t e = 0;
						while (!running_task.compare_exchange_strong(
							e, e + 1
						))
						{

						}
						task_context.CommitTask(this, new_pro);
					}
				}
			}
		}
	}

	void Context::Release()
	{
		auto o_resource = m_resource;
		this->~Context();
		o_resource->deallocate(
			this,
			sizeof(Context),
			alignof(Context)
		);
	}

	bool Context::StartLoop()
	{
		std::lock_guard lg(property_mutex);
		std::size_t E = 0;
		if(task_context && running_task.compare_exchange_strong(E, 2))
		{
			if(!task_context->CommitTask(this, {
				config.priority,
				TaskName,
				0
			}))
			{
				E = 2;
				auto re = running_task.compare_exchange_strong(E, 0);
				assert(re);
				return false;
			}
			return true;
		}
		return false;
	}

	std::optional<std::size_t> Context::CheckConflict(System::Priority priority,
		System::Property sys_property,
		System::MutexProperty mutex_property)
	{
		for (auto& ite : tick_systems)
		{
			if (ite.property.IsSameSystem(sys_property))
				return std::nullopt;
		}

		std::size_t old_dependence_size = tick_systems_graphic_line.size();

		std::size_t tar_index = tick_systems.size();

		std::size_t i = 0;

		bool has_to_node = false;
		std::size_t in_degree = 0;

		for (auto& ite : tick_systems)
		{
			if(ite.priority.layer == priority.layer)
			{
				if (mutex_property.IsConflig(ite.mutex_property))
				{
					auto K = priority.CompareCustomPriority(
						sys_property, ite.priority, ite.property
					);

					if (K == std::partial_ordering::greater)
					{
						tick_systems_graphic_line.push_back({
							false, priority.layer, tar_index, i
							});
						has_to_node = true;
					}
					else if (K == std::partial_ordering::less)
					{
						tick_systems_graphic_line.push_back({
							false, priority.layer,i, tar_index
						});
						in_degree += 1;
					}
					else if (K == std::partial_ordering::equivalent)
					{
						tick_systems_graphic_line.push_back({
							true, priority.layer,i, tar_index
						});
						tick_systems_graphic_line.push_back({
							true, priority.layer,tar_index, i
						});
					}
					else
					{
						tick_systems_graphic_line.resize(old_dependence_size);
						return std::nullopt;
					}
				}
			}
			++i;
		}

		if(has_to_node && in_degree != 0)
		{
			auto f1 = std::find_if(tick_systems_graphic_line.begin(), tick_systems_graphic_line.end(), [=](GraphicLine const& l)
			{
				return l.layer == priority.layer;
			});

			auto f2 = std::find_if(tick_systems_graphic_line.begin(), tick_systems_graphic_line.end(), [=](GraphicLine const& l)
			{
				return l.layer > priority.layer;
			});

			auto old_span = std::span(f1, f2);
			auto new_span = std::span(tick_systems_graphic_line).subspan(old_dependence_size);

			std::vector<std::size_t> search_stack;
			for(auto& ite : new_span)
			{
				if(!ite.is_mutex && ite.from_node == tar_index)
				{
					search_stack.push_back(ite.to_node);
				}
			}

			while(!search_stack.empty())
			{
				auto top = *search_stack.rbegin();
				search_stack.pop_back();
				for(auto& ite : old_span)
				{
					if(!ite.is_mutex && ite.from_node == top)
					{
						auto tar = ite.to_node;
						for(auto& ite2 : new_span)
						{
							if(!ite2.is_mutex && ite2.from_node == tar)
							{
								tick_systems_graphic_line.resize(old_dependence_size);
								return std::nullopt;
							}
						}

						auto find = std::find(search_stack.begin(), search_stack.end(), tar);
						if(find == search_stack.end())
							search_stack.push_back(tar);
					}
				}
			}

			for(auto& ite : new_span)
			{
				if(!ite.is_mutex && ite.to_node != tar_index)
				{
					assert(ite.to_node < tick_systems.size());
					tick_systems[ite.to_node].in_degree += 1;
				}
			}
		}

		return in_degree;
	}

	bool Context::AddRawTickSystem(
		System::Priority priority,
		System::Property sys_property,
		System::MutexProperty mutex_property,
		System::Object&& obj
	)
	{
		if (!obj || sys_property.system_name.empty())
			return false;

		std::lock_guard lg(tick_system_mutex);

		auto in_degree = CheckConflict(
			priority,
			sys_property,
			mutex_property
		);

		if (in_degree.has_value())
		{
			auto find = std::find_if(
				tick_systems.begin(),
				tick_systems.end(),
				[=](SystemStorage const& ss)
				{
					return ss.priority.layer < priority.layer;
				}
			);

			std::size_t dis = std::distance(tick_systems.begin(), find);

			auto tar_index = tick_systems.size();

			for(auto& ite : tick_systems_graphic_line)
			{
				if(ite.from_node == tar_index)
					ite.from_node = dis;
				else if(ite.from_node >= dis)
					ite.from_node += 1;

				if (ite.to_node == tar_index)
					ite.to_node = dis;
				else if (ite.to_node >= dis)
					ite.to_node += 1;
			}

			tick_systems.emplace(
				find,
				priority,
				sys_property,
				mutex_property,
				std::move(obj),
				*in_degree
			);

			std::sort(
				tick_systems_graphic_line.begin(),
				tick_systems_graphic_line.end(),
				[](GraphicLine const& i1, GraphicLine const& i2)
				{
					if(i1.layer == i2.layer)
					{
						return i1.from_node < i2.from_node;
					}else
						return i1.layer > i2.layer;
				}
			);

			need_refresh_dependence = true;

			return true;
		}

		return false;
	}

	void Context::FlushTickSystem()
	{
		if (need_refresh_dependence)
		{
			if(tick_system_running_mutex.try_lock())
			{
				std::lock_guard lg(tick_system_running_mutex, std::adopt_lock);
				need_refresh_dependence = false;

				tick_system_context.clear();
				tick_system_context.reserve(tick_systems.size());

				for(auto& ite : tick_systems)
				{
					tick_system_context.emplace_back(
						SystemStatus::Ready,
						ite.object.object,
						ite.property,
						ite.priority.layer,
						std::span<GraphicLine const> {},
						std::span<GraphicLine const>{},
						ite.in_degree
					);
				}

				tick_systems_running_graphic_line.clear();
				auto gl_size = tick_systems_graphic_line.size();
				tick_systems_running_graphic_line.reserve(gl_size * 2);

				tick_systems_running_graphic_line.insert(
					tick_systems_running_graphic_line.end(),
					tick_systems_graphic_line.begin(),
					tick_systems_graphic_line.end()
				);

				tick_systems_running_graphic_line.insert(
					tick_systems_running_graphic_line.end(),
					tick_systems_graphic_line.begin(),
					tick_systems_graphic_line.end()
				);

				auto normal_span = std::span(tick_systems_running_graphic_line).subspan(0, gl_size);
				auto reverse_span = std::span(tick_systems_running_graphic_line).subspan(gl_size);

				for(auto& ite : reverse_span)
				{
					std::swap(ite.from_node, ite.to_node);
				}

				std::sort(reverse_span.begin(), reverse_span.end(), [](GraphicLine const& l1, GraphicLine const& l2)
				{
					if(l1.layer == l2.layer)
					{
						if(l1.from_node == l2.from_node)
						{
							return l1.to_node < l2.to_node;
						}else
							return l1.from_node < l2.from_node;
					}else
						return l1.layer > l2.layer;
				});

				auto start = normal_span.begin();
				auto ite = start;
				while(ite != normal_span.end())
				{
					if(ite->from_node != start->from_node)
					{
						tick_system_context[start->from_node].graphic_line = {start, ite};
						start = ite;
					}
					++ite;
				}
				if(start != ite && start != normal_span.end())
				{
					tick_system_context[start->from_node].graphic_line = { start, ite };
				}

				start = reverse_span.begin();
				ite = start;
				while (ite != reverse_span.end())
				{
					if (ite->from_node != start->from_node)
					{
						tick_system_context[start->from_node].reverse_graphic_line = { start, ite };
						start = ite;
					}
					++ite;
				}
				if (start != ite && start != reverse_span.end())
				{
					tick_system_context[start->from_node].reverse_graphic_line = { start, ite };
				}
			}
		}
	}


	void Context::InitTickSystem()
	{
		for(auto& ite : tick_system_context)
			ite.status = SystemStatus::Ready;
		current_layer = 0;
		systems_count = 0;
		runs_system_count = 0;
		if(!tick_system_context.empty())
		{
			current_layer = tick_system_context[0].layer;
			for(auto& ite : tick_system_context)
			{
				if(ite.layer == current_layer)
					++systems_count;
				else
					break;
			}
		}
		current_layer_system_count = systems_count;
	}

	/*
	bool Context::AddRawSystem(
		SystemObject const& object,
		std::span<SystemRWInfo const> rw_infos,
		SystemProperty system_property,
		std::partial_ordering (*priority_detect)(SystemProperty const&, SystemProperty const&)
	)
	{
		bool enable_insert = true;
		{
			std::lock_guard lg(system_mutex);
			for(auto& ite : systems)
			{
				if(ite.property.group_name == system_property.group_name 
					&& ite.property.system_name == ite.property.system_name)
				{
					enable_insert = false;
					break;
				}
			}
		}
		if(enable_insert)
		{
			pending_system.push_back(
				
			);
		}
	}
	*/
}