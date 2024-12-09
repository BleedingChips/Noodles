module;

#include <cassert>
#include <Windows.h>

module NoodlesContext;
import PotatoFormat;

namespace Noodles
{

	void SystemNode::TaskFlowNodeExecute(Potato::Task::TaskFlowContext& status)
	{
		LayerTaskFlow* flow = static_cast<LayerTaskFlow*>(status.flow.GetPointer());
		Context* Tar = nullptr;

		{
			std::lock_guard lg(flow->process_mutex);
			Tar = static_cast<Context*>(flow->parent_node.GetPointer());
		}

		ContextWrapper exe_context
		{
			status.context,
			status.node_property,
			*Tar,
			*flow,
			static_cast<SystemNode&>(*status.current_node),
			status.node_identity,
			{}
		};

		SystemNodeExecute(exe_context);
	}

	bool LayerTaskFlow::AddTickedNode(SystemNode::Ptr node, SystemName system_name, SystemNodeProperty property, std::pmr::memory_resource* temp_resource)
	{
		if(node)
		{
			std::lock_guard lg(preprocess_mutex);

			std::size_t index = 0;

			for(;index < system_infos.size(); ++index)
			{
				if(!system_infos[index].node)
					break;
			}

			if(index == system_infos.size())
				system_infos.emplace_back();

			auto worst_node = worst_graph.Add(index);
			assert(worst_node);

			Potato::Task::TaskFlowNodeProperty nproperty
			{
				system_name.name,
				property.filter
			};

			auto task_node = TaskFlow::AddNode_AssumedLocked(node.GetPointer(), nproperty, index);
			assert(task_node);

			auto cur_mutex = node->GetMutex();

			bool accept = true;

			for(auto& ite : system_infos)
			{
				if(ite.node)
				{
					bool component_overlapping = false;
					bool singleton_overlapping = false;
					auto ite_mutex = ite.node->GetMutex();

					bool thread_order_overlapping = cur_mutex.thread_order_mark.WriteConfig(ite_mutex.thread_order_mark);

					if(!thread_order_overlapping)
					{
						component_overlapping = cur_mutex.component_mark.WriteConfig(ite_mutex.component_mark);
						singleton_overlapping = cur_mutex.singleton_mark.WriteConfig(ite_mutex.singleton_mark);
					}

					if (thread_order_overlapping || component_overlapping || singleton_overlapping)
					{
						auto num_order = property.priority <=> ite.priority;
						bool need_worth_test = (num_order == std::strong_ordering::equal);

						if (num_order == std::strong_ordering::equal)
						{
							auto display_name = system_name;
							Order o1 = Order::UNDEFINE;
							Order o2 = Order::UNDEFINE;
							if (property.order_function != nullptr)
							{
								o1 = (*property.order_function)(display_name, ite.name);
							}
							if (ite.order_func != nullptr && ite.order_func != property.order_function)
							{
								o2 = (*ite.order_func)(ite.name, display_name);
							}
							if (
								o1 == Order::BIGGER && o2 == Order::SMALLER
								|| o1 == Order::SMALLER && o2 == Order::BIGGER
								|| o1 == Order::UNDEFINE && o2 == Order::UNDEFINE
								)
							{
								accept = false;
								break;
							}
							else if (o1 == Order::UNDEFINE)
							{
								o1 = o2;
							}
							if (o1 == Order::SMALLER)
							{
								num_order = std::strong_ordering::less;
							}
							else if (o1 == Order::BIGGER)
							{
								num_order = std::strong_ordering::greater;
							}
							else if (o1 == Order::MUTEX)
							{
							}
							else if (o1 == Order::UNDEFINE)
							{
								accept = false;
								break;
							}
						}

						if(num_order == std::strong_ordering::equal)
						{
							if (thread_order_overlapping)
							{
								AddMutexEdge_AssumedLocked(ite.task_node, task_node, { false });
							}
							else
							{
								dynamic_edges.emplace_back(
									true,
									component_overlapping,
									singleton_overlapping,
									task_node,
									ite.task_node
								);
							}
						}
						else if (num_order == std::strong_ordering::greater)
						{
							if (need_worth_test && !worst_graph.AddEdge(worst_node, ite.worst_graph_node, { false }, temp_resource))
							{
								accept = false;
								break;
							}
							else
							{
								if (thread_order_overlapping)
								{
									AddDirectEdge_AssumedLocked(task_node, ite.task_node, { false });
								}
								else
								{
									dynamic_edges.emplace_back(
										false,
										component_overlapping,
										singleton_overlapping,
										task_node,
										ite.task_node
									);
								}
							}
						}
						else if (num_order == std::strong_ordering::less)
						{
							if (need_worth_test && !worst_graph.AddEdge(ite.worst_graph_node, worst_node, { false }, temp_resource))
							{
								accept = false;
								break;
							}
							else
							{
								if (thread_order_overlapping)
								{
									AddDirectEdge_AssumedLocked(ite.task_node, task_node, { false });
								}
								else
								{
									dynamic_edges.emplace_back(
										false,
										component_overlapping,
										singleton_overlapping,
										ite.task_node,
										task_node
									);
								}
							}
						}
					}
				}
			}

			if (!accept)
			{
				worst_graph.RemoveNode(worst_node);
				Remove_AssumedLocked(task_node);
				return false;
			}

			auto& ref = system_infos[index];

			ref.name = system_name;
			ref.node = std::move(node);
			ref.order_func = property.order_function;
			ref.priority = property.priority;
			ref.task_node = task_node;
			ref.worst_graph_node = worst_node;

			return true;
		}
		return false;
	}

	bool LayerTaskFlow::AddTemporaryNodeImmediately(SystemNode::Ptr node, std::u8string_view system_name, Potato::Task::TaskFilter filter)
	{
		std::lock_guard lg(process_mutex);
		return AddTemporaryNodeImmediately_AssumedLocked(node, system_name, filter);
	}

	bool LayerTaskFlow::AddTemporaryNodeImmediately_AssumedLocked(SystemNode::Ptr node, std::u8string_view system_name, Potato::Task::TaskFilter filter)
	{
		if(finished_task >= process_nodes.size())
			return false;

		auto rw_mutex = node->GetMutex();
		auto name = system_name;

		std::shared_lock sl(context_ptr->component_manager.GetMutex());
		std::shared_lock s2(context_ptr->singleton_manager.GetMutex());

		auto archetype_usage = context_ptr->component_manager.GetArchetypeUsageMark_AssumedLocked();
		auto singleton_usage = context_ptr->singleton_manager.GetSingletonUsageMark_AssumedLocked();


		if(TaskFlow::AddTemporaryNode_AssumedLocked(
			node.GetPointer(),
			{ system_name, filter},
			[=, this](TaskFlowNode const& node, Potato::Task::TaskFlowNodeProperty property, TemporaryNodeIndex index)-> bool
			{
				auto& sys = static_cast<SystemNode const&>(node);

				auto s_rw_mutex = sys.GetMutex();

				bool is_overlap = rw_mutex.thread_order_mark.WriteConfig(s_rw_mutex.thread_order_mark);

				if (!is_overlap)
				{
					is_overlap = rw_mutex.component_mark.WriteConfig(s_rw_mutex.component_mark);
				}

				if (!is_overlap)
				{
					is_overlap = rw_mutex.singleton_mark.WriteConfig(s_rw_mutex.singleton_mark);
				}

				return is_overlap;
			}
		))
		{
			return true;
		}
		return false;
	}

	bool LayerTaskFlow::AddTemporaryNodeDefer(SystemNode::Ptr node, std::u8string_view system_name, Potato::Task::TaskFilter filter)
	{
		std::lock_guard lg(process_mutex);
		if (node)
		{
			defer_temporary_system_node.emplace_back(std::move(node), system_name, filter);
			return true;
		}
		return false;
	}

	bool LayerTaskFlow::Update_AssumedLocked(std::pmr::memory_resource* resource)
	{
		bool has_archetype_update = context_ptr->component_manager.HasArchetypeUpdate_AssumedLocked();
		if (has_archetype_update || context_ptr->this_frame_singleton_update)
		{
			for (auto& ite : dynamic_edges)
			{
				auto from = static_cast<SystemNode*>(preprocess_nodes[ite.pre_process_from].node.GetPointer());
				auto to = static_cast<SystemNode*>(preprocess_nodes[ite.pre_process_to].node.GetPointer());
				bool need_edge = false;
				if (context_ptr->this_frame_singleton_update && ite.singleton_overlapping)
				{
					auto mux1 = from->GetMutex();
					auto mux2 = to->GetMutex();

					if(
						mux1.singleton_mark.WriteConfigWithMask(mux2.singleton_mark, context_ptr->singleton_manager.GetSingletonUsageMark_AssumedLocked())
					)
					{
						need_edge = true;
					}
				}
				if (!need_edge && has_archetype_update && ite.component_overlapping)
				{
					if (from->IsComponentOverlapping(*to, context_ptr->component_manager.GetArchetypeUpdateMark_AssumedLocked(), context_ptr->component_manager.GetArchetypeUsageMark_AssumedLocked()))
					{
						need_edge = true;
					}
				}
				if (need_edge)
				{
					AddDirectEdge_AssumedLocked(ite.pre_process_from, ite.pre_process_to);
				}else
				{
					RemoveDirectEdge_AssumedLocked(ite.pre_process_from, ite.pre_process_to);
				}
			}
		}
		auto updateed = TaskFlow::Update_AssumedLocked(resource);
		for(auto& ite : defer_temporary_system_node)
		{
			auto re = AddTemporaryNodeImmediately_AssumedLocked(ite.ptr, ite.system_name, ite.filter);
			assert(re);
		}
		defer_temporary_system_node.clear();
		return false;
	}

	struct ParallelExecutor : public Potato::Task::Task, Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<ParallelExecutor>;

		bool TryCommited(Potato::Task::TaskContext& context, Potato::Task::TaskFlowNodeProperty property, std::size_t user_index);
		ParallelExecutor(Potato::IR::MemoryResourceRecord record) : MemoryResourceRecordIntrusiveInterface(record) {}
		~ParallelExecutor() { assert(!reference_context); }

		virtual void TaskExecute(Potato::Task::TaskContextWrapper& status) override;

		
		void AddTaskRef() const override { MemoryResourceRecordIntrusiveInterface::AddRef(); }
		void SubTaskRef() const override { MemoryResourceRecordIntrusiveInterface::SubRef(); }
		

		Context::Ptr reference_context;
		LayerTaskFlow::Ptr current_flow;
		SystemNode::Ptr reference_node;
		std::size_t node_index = 0;

		std::size_t total_count = 0;
		std::atomic_size_t waiting_task = 0;
		std::atomic_size_t finish_task = 0;
	};

	bool ContextWrapper::CommitParallelTask(std::size_t user_index, std::size_t total_count, std::size_t executor_count, std::pmr::memory_resource* resource)
	{
		if(parallel_info.status != ParallelInfo::Status::Parallel && total_count > 0)
		{
			if(current_layout_flow.MarkNodePause(node_index))
			{
				ParallelExecutor::Ptr re = Potato::IR::MemoryResourceRecord::AllocateAndConstruct<ParallelExecutor>(resource);
				if(re)
				{
					re->current_flow = &current_layout_flow;
					re->reference_node = &current_node;
					re->reference_context = &noodles_context;
					re->total_count = total_count;
					re->node_index = node_index;
					for(std::size_t i = 0; i < executor_count; ++i)
					{
						auto res = re->TryCommited(task_context, node_property, user_index);
					}

					return true;
				}else
				{
					current_layout_flow.ContinuePauseNode(task_context, node_index);
				}
			}
		}
		return false;
	}

	void Context::Quit()
	{
		std::lock_guard lg(mutex);
		require_quit = true;
	}

	Context::Context(Config config)
		: config(config)
	{
		entity_manager.Init(component_manager);
	}

	LayerTaskFlow* Context::FindSubContextTaskFlow_AssumedLocked(std::int32_t layer)
	{
		for (auto& ite : preprocess_nodes)
		{
			auto ptr = static_cast<LayerTaskFlow*>(ite.node.GetPointer());
			assert(ptr != nullptr);
			if (ptr->layout == layer)
			{
				return ptr;
			}
		}
		return nullptr;
	}

	LayerTaskFlow::Ptr Context::FindOrCreateContextTaskFlow_AssumedLocked(std::int32_t layer)
	{
		auto ptr = FindSubContextTaskFlow_AssumedLocked(layer);
		if(ptr == nullptr)
		{
			auto re = Potato::IR::MemoryResourceRecord::Allocate<LayerTaskFlow>(std::pmr::get_default_resource());
			if (re)
			{
				LayerTaskFlow::Ptr ptr = new(re.Get()) LayerTaskFlow{ re, layer, this };
				auto cur_node = AddNode_AssumedLocked(ptr.GetPointer(), { u8"sub_task" });
				for (auto& ite : preprocess_nodes)
				{
					auto tar = static_cast<LayerTaskFlow*>(ite.node.GetPointer());
					if (tar != ptr)
					{
						if (tar->layout > layer)
						{
							auto re = AddDirectEdge_AssumedLocked(ite.self, cur_node);
							assert(re);
						}
						else
						{
							assert(tar->layout != layer);
							auto re = AddDirectEdge_AssumedLocked(cur_node, ite.self);
							assert(re);
						}
					}
				}
				return ptr;
			}
		}
		
		return ptr;
	}

	bool Context::AddTickedSystemNode(SystemNode::Ptr node, SystemName system_name, SystemNodeProperty property)
	{
		if (node)
		{
			std::lock_guard lg(preprocess_mutex);
			auto ptr = FindOrCreateContextTaskFlow_AssumedLocked(property.priority.layout);
			if (ptr)
			{
				return ptr->AddTickedNode(std::move(node), system_name, property);
			}
		}
		return false;
	}

	bool Context::AddTemporarySystemNodeDefer(SystemNode::Ptr node, std::int32_t layout, std::u8string_view system_name, Potato::Task::TaskFilter filter)
	{
		std::lock_guard lg(preprocess_mutex);
		auto ptr = FindOrCreateContextTaskFlow_AssumedLocked(layout);
		if (ptr)
		{
			return ptr->AddTemporaryNodeDefer(std::move(node), system_name, filter);
		}
		return false;
	}

	bool Context::Update_AssumedLocked(std::pmr::memory_resource* resource)
	{
		component_manager.ClearArchetypeUpdateMark_AssumedLocked();
		entity_manager.Flush(component_manager, resource);
		this_frame_singleton_update = singleton_manager.Flush(resource);
		return TaskFlow::Update_AssumedLocked(resource);
	}

	void Context::TaskFlowExecuteBegin(Potato::Task::TaskFlowContext& context)
	{
		std::lock_guard lg(mutex);
		auto cur = std::chrono::steady_clock::now();
		if(start_up_tick_lock.time_since_epoch().count() != 0)
			framed_duration = cur - start_up_tick_lock;
		start_up_tick_lock = cur;
	}

	void Context::TaskFlowExecuteEnd(Potato::Task::TaskFlowContext& context)
	{
		std::chrono::steady_clock::time_point now_time = std::chrono::steady_clock::now();
		std::chrono::steady_clock::time_point require_time;
		{
			std::lock_guard lg(mutex);
			if (require_quit)
			{
				require_quit = false;
				return;
			}
			require_time = start_up_tick_lock + config.min_frame_time;
		}
		entity_manager.Flush(component_manager);
		singleton_manager.Flush();
		Update();

		TaskFlow::Commited(context.context, require_time, context.node_property);
	}

	ThreadOrderFilter::Ptr Context::CreateThreadOrderFilter(std::span<StructLayoutWriteProperty const> info, std::pmr::memory_resource* resource)
	{
		auto layout = Potato::MemLayout::MemLayoutCPP::Get<ThreadOrderFilter>();
		auto offset = layout.Insert(Potato::MemLayout::Layout::Get<MarkElement>(), thread_order_manager.GetStorageCount() * 2);
		auto re = Potato::IR::MemoryResourceRecord::Allocate(resource, layout.Get());
		if (re)
		{
			std::span<MarkElement> write_span = {
				new(re.GetByte(offset)) MarkElement[thread_order_manager.GetStorageCount()],
				thread_order_manager.GetStorageCount()
			};

			std::span<MarkElement> total_span = {
				new(re.GetByte(offset) + sizeof(MarkElement) * thread_order_manager.GetStorageCount()) MarkElement[thread_order_manager.GetStorageCount()],
				thread_order_manager.GetStorageCount()
			};

			for (auto& ite : info)
			{
				auto loc = thread_order_manager.LocateOrAdd(ite.struct_layout);
				if (!loc)
				{
					re.Deallocate();
					return {};
				}else
				{
					if (ite.need_write)
					{
						MarkElement::Mark(write_span, *loc);
					}
					MarkElement::Mark(total_span, *loc);
				}
			}

			return new (re.Get()) ThreadOrderFilter{ re, {write_span, total_span} };
		}
		return {};
	}

	bool Context::Commited(Potato::Task::TaskContext& context, Potato::Task::TaskFlowNodeProperty property)
	{
		std::lock_guard lg(TaskFlow::process_mutex);
		if(current_status == Status::DONE || current_status == Status::READY)
		{
			Update_AssumedLocked();
			return TaskFlow::Commited_AssumedLocked(context, std::move(property));
		}
		return false;
	}

	void ParallelExecutor::TaskExecute(Potato::Task::TaskContextWrapper& status)
	{
		ContextWrapper exe_context
		{
			status.context,
			{status.task_property.display_name, status.task_property.filter},
			*reference_context,
			*current_flow,
			*reference_node,
			node_index,
			ParallelInfo{
				ParallelInfo::Status::Parallel,
				total_count,
				status.task_property.user_data[0],
				status.task_property.user_data[1]
			}
		};
		reference_node->SystemNodeExecute(exe_context);
		std::size_t desird = 0;
		while(!finish_task.compare_exchange_weak(desird, desird + 1))
		{
			
		}
		if(desird == total_count - 1)
		{
			ContextWrapper exe_context2
			{
				status.context,
				{status.task_property.display_name, status.task_property.filter},
				*reference_context,
				*current_flow,
				*reference_node,
				node_index,
				ParallelInfo{
					ParallelInfo::Status::Parallel,
					total_count,
					status.task_property.user_data[0],
					status.task_property.user_data[1]
				}
			};
			reference_node->SystemNodeExecute(exe_context2);
			auto flow = std::move(current_flow);
			reference_context.Reset();
			current_flow.Reset();
			reference_node.Reset();
			flow->ContinuePauseNode(status.context, node_index);
			
		}else if(!TryCommited(status.context, {status.task_property.display_name, status.task_property.filter}, status.task_property.user_data[1] ))
		{
		}
	}

	bool ParallelExecutor::TryCommited(Potato::Task::TaskContext& context, Potato::Task::TaskFlowNodeProperty property, std::size_t user_index)
	{
		std::size_t desird = 0;
		while(!waiting_task.compare_exchange_weak(desird, desird + 1))
		{
			if(desird >= total_count)
				break;
		};
		if(desird < total_count)
		{
			auto re = context.CommitTask(this, {
				property.display_name, {desird, user_index}, property.filter
			});
			assert(re);
			return true;
		}
		return false;
	}
}