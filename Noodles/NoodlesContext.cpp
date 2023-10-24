module;

#include <cassert>

module NoodlesContext;

namespace Noodles
{

	/*
	constexpr std::u8string_view TaskName = u8"Noodles Startup";

	auto Context::Create(ContextConfig config, Potato::Task::TaskContext::Ptr TaskPtr, std::pmr::memory_resource* UpstreamResource)->Ptr
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

	Context::Context(ContextConfig config, Potato::Task::TaskContext::Ptr TaskPtr, std::pmr::memory_resource* Resource)
		: task_context(std::move(TaskPtr)), m_resource(Resource), config(config)
	{
	}

	Context::~Context()
	{
		
	}

	void Context::operator()(Potato::Task::ExecuteStatus& status)
	{
		System::RunningContext* ptr = reinterpret_cast<System::RunningContext*>(status.Property.AppendData);

		if(ptr == nullptr)
		{
			{
				std::lock_guard lg2(property_mutex);
				last_execute_time = std::chrono::system_clock::now();
			}

			{
				std::lock_guard lg(tick_system_mutex);
				std::lock_guard lg2(tick_system_running_mutex);
				FlushAndInitTickSystem();
				TryFireNextLevelZeroDegreeTickSystem(status.Context);
			}
		}else
		{
			if(status.Property.AppendData2 == 1)
			{
				System::ExecuteContext Cont{
				ptr->property,
				*this
				};
				ptr->Execute(
					Cont
				);
				status.Property.AppendData2 = 2;
			}

			if (tick_system_running_mutex.try_lock())
			{
				std::lock_guard lg(tick_system_running_mutex, std::adopt_lock);
				assert(ptr->status == System::RunningStatus::Waiting);
				ptr->status = System::RunningStatus::Done;
				--current_level_system_waiting;
				TryFireBeDependenceTickSystem(status.Context, ptr);
				if(current_level_system_waiting == 0)
				{
					TryFireNextLevelZeroDegreeTickSystem(status.Context);
				}
			}
			else
			{
				status.Context.CommitTask(this, status.Property);
				return;
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
			System::RunningContext* con = nullptr;
			new_pro.AppendData = reinterpret_cast<std::size_t>(con);
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

	void Context::TryFireNextLevelZeroDegreeTickSystem(Potato::Task::TaskContext& context)
	{
		auto span = std::span(startup_system_context).subspan(starup_up_system_context_ite);
		std::optional<std::size_t> req_layout;
		for(auto& ite : span)
		{
			if(!req_layout.has_value() || *req_layout == ite.layout)
			{
				req_layout = ite.layout;
				FireSingleTickSystem(context, ite.system_obj);
				++starup_up_system_context_ite;
			}else
			{
				break;
			}
		}
	}

	void Context::TryFireBeDependenceTickSystem(Potato::Task::TaskContext& context, System::RunningContext* ptr)
	{
		assert(ptr != nullptr);

		auto span = ptr->reference_trigger_line.Slice(std::span(tick_systems_running_graphic_line));

		for(auto& ite : span)
		{
			if(ite.is_mutex)
			{
				assert(ite.target->mutex_degree != 0);
				ite.target->mutex_degree -= 1;
			}else{
				assert(ite.target->current_in_degree != 0);
				ite.target->current_in_degree -= 1;
			}
			if (ite.target->status == System::RunningStatus::Ready
				&& ite.target->mutex_degree == 0
				&& ite.target->current_in_degree == 0
				)
			{
				FireSingleTickSystem(context, ite.target);
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
			System::RunningContext* ptr = nullptr;
			if(!task_context->CommitTask(this, {
				config.priority,
				TaskName,
				reinterpret_cast<std::size_t>(ptr)
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


	void Context::FireSingleTickSystem(Potato::Task::TaskContext& context, System::RunningContext* ptr)
	{
		assert(ptr != nullptr);

		assert(ptr->status == System::RunningStatus::Ready);

		auto span = std::span(tick_systems_running_graphic_line);

		for(auto ite : span)
		{
			if(ite.is_mutex)
			{
				ite.target->mutex_degree += 1;
			}
		}

		std::size_t e = 0;
		while (!running_task.compare_exchange_strong(
			e, e + 1
		))
		{

		}
		current_level_system_waiting += 1;

		ptr->status = System::RunningStatus::Waiting;
		Potato::Task::TaskProperty new_pro{
			ptr->property.task_priority,
			ptr->property.system_name,
			reinterpret_cast<std::size_t>(ptr),
			1
		};
		context.CommitTask(this, new_pro);
	}

	auto Context::TickSystemDependenceCheck(System::Property const& pro, System::Priority const& priority, System::MutexProperty const& mutex_property)
		->CircleDependenceCheckResult
	{
		if(!pro.system_name.empty())
		{
			bool has_to_node = false;
			std::size_t in_degree = 0;

			std::pmr::vector<NewLogicDependenceLine> new_line;

			auto find = std::find_if(
				tick_systems.begin(),
				tick_systems.end(),
				[=](LogicSystemRunningContext const& C)
				{
					return C.priority.layer <= priority.layer;
				}
			);

			auto find2 = std::find_if(
				find,
				tick_systems.end(),
				[=](LogicSystemRunningContext const& C)
				{
					return C.priority.layer < priority.layer;
				}
			);

			auto span = std::span<LogicSystemRunningContext>{ find, find2 };

			std::size_t tar_index = std::distance(tick_systems.begin(), find2);
			std::size_t i = std::distance(tick_systems.begin(), find);

			for (auto& ite : span)
			{
				if (!ite.property.IsSameSystem(pro))
				{
					if (ite.mutex_property.IsConflict(mutex_property))
					{
						auto K = priority.CompareCustomPriority(
							pro, ite.priority, ite.property
						);

						if (K == std::partial_ordering::greater)
						{
							new_line.push_back({
								false, tar_index, i
								});
							has_to_node = true;
						}
						else if (K == std::partial_ordering::less)
						{
							new_line.push_back({
								false, i, tar_index
								});
							in_degree += 1;
						}
						else if (K == std::partial_ordering::equivalent)
						{
							new_line.push_back({
								true, i, tar_index
								});
							new_line.push_back({
								true, tar_index, i
								});
						}
						else
						{
							return {
								CircleDependenceCheckResult::Status::ConfuseDependence
							};
						}
					}
				}
				else
				{
					return {
						CircleDependenceCheckResult::Status::ExistName
					};
				}
				++i;
			}

			if (has_to_node && in_degree != 0)
			{
				for (auto& ite : new_line)
				{
					if (!ite.is_mutex && ite.from_node == tar_index)
					{
						if (RecursionSearchNode(ite.to_node, tar_index, std::span(new_line)))
						{
							return {
								CircleDependenceCheckResult::Status::CircleDependence
							};
						}
					}
				}
			}

			return {
				CircleDependenceCheckResult::Status::Available,
				tar_index,
				in_degree,
				std::move(new_line)
			};

		}else
		{
			return {
				CircleDependenceCheckResult::Status::EmptyName
			};
		}
	}

	void Context::TickSystemInsert(CircleDependenceCheckResult const& result, System::Property const& pro, System::Priority const& priority, System::MutexProperty const& mutex_property, System::RunningContext* context)
	{
		assert(result);
		assert(context != nullptr);
		context->startup_in_degree = result.in_degree;
		context->property = pro;
		context->status = System::RunningStatus::PreInit;

		for (auto& ite : result.dependence_line)
		{
			if (!ite.is_mutex && ite.to_node != result.ite_index)
			{
				assert(ite.to_node < tick_systems.size());
				tick_systems[ite.to_node].in_degree += 1;
			}
		}

		auto insert_ite = tick_systems.begin() + result.ite_index;

		for (auto ite = insert_ite; ite != tick_systems.end(); ++ite)
		{
			auto old_span = std::span(ite->dependence_line);
			for (auto& ite : old_span)
			{
				if (ite.to_node >= result.ite_index)
				{
					ite.to_node += 1;
				}
			}
		}

		tick_systems.emplace(
			insert_ite,
			pro,
			priority,
			mutex_property,
			context,
			result.in_degree,
			std::pmr::vector<LogicDependenceLine>{m_resource}
		);

		for (auto& ite : result.dependence_line)
		{
			tick_systems[ite.from_node].dependence_line.emplace_back(
				ite.is_mutex,
				ite.to_node
			);
		}

		need_refresh_dependence = true;
	}

	bool Context::RecursionSearchNode(std::size_t cur, std::size_t target, std::span<NewLogicDependenceLine> Line)
	{
		for(auto& ite : Line)
		{
			if(!ite.is_mutex && ite.from_node == cur && ite.to_node == target)
				return true;
		}

		auto cur_span = std::span(tick_systems[cur].dependence_line);

		for(auto& ite : cur_span)
		{
			if(!ite.is_mutex)
			{
				if(!RecursionSearchNode(ite.to_node, target, Line))
					return true;
			}
		}

		return false;
	}

	void Context::FlushAndInitTickSystem()
	{
		if(!need_refresh_dependence)
		{
			for (auto& ite : tick_systems)
			{
				auto ptr = ite.system_obj;
				ptr->startup_in_degree = ite.in_degree;
				ptr->current_in_degree = ite.in_degree;
				ptr->status = System::RunningStatus::Ready;
				ptr->mutex_degree = 0;
			}
			starup_up_system_context_ite = 0;
			current_level_system_waiting = 0;
		}else
		{
			need_refresh_dependence = false;

			startup_system_context.clear();
			tick_systems_running_graphic_line.clear();

			for(auto& ite : tick_systems)
			{
				auto ptr = ite.system_obj;
				ptr->startup_in_degree = ite.in_degree;
				ptr->current_in_degree = ite.in_degree;
				ptr->status = System::RunningStatus::Ready;
				ptr->mutex_degree = 0;
				auto cur_index = tick_systems_running_graphic_line.size();
				for(auto& ite2 : ite.dependence_line)
				{
					tick_systems_running_graphic_line.emplace_back(
					ite2.is_mutex,
					tick_systems[ite2.to_node].system_obj
					);
				}
				auto cur_index2 = tick_systems_running_graphic_line.size();
				ptr->reference_trigger_line = { cur_index, cur_index2 };
				if(ite.in_degree == 0)
				{
					startup_system_context.emplace_back(
						ite.priority.layer,
						ptr
					);
				}
			}
			starup_up_system_context_ite = 0;
			current_level_system_waiting = 0;
		}
	}
	*/
}