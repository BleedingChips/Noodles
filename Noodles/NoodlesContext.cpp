module;

#include <cassert>
#include <Windows.h>

module NoodlesContext;
import PotatoFormat;

namespace Noodles
{

	void ContextWrapper::AddTemporarySystemNodeNextFrame(SystemNode& node, Potato::Task::Property property)
	{
		layer_flow.AddTemporaryNodeNextFrame(node, std::move(property));
	}

	void ContextWrapper::AddTemporarySystemNode(SystemNode& node, Potato::Task::Property property)
	{
		layer_flow.AddTemporaryNode(context, flow_context, node, std::move(property));
	}

	void SystemNode::TaskExecute(Potato::Task::ContextWrapper& wrapper)
	{
		auto layer_flow = dynamic_cast<LayerTaskFlow*>(wrapper.GetTriggerProperty().trigger.GetPointer());
		assert(layer_flow != nullptr);
		auto flow_context = wrapper.GetTriggerProperty().data.TryGetNodeDataPointerWithType<Potato::TaskGraphic::FlowProcessContext>();
		assert(flow_context != nullptr);
		auto context = dynamic_cast<Context*>(flow_context->GetTriggerProperty().trigger.GetPointer());
		assert(context != nullptr);
		ContextWrapper context_wrapper{wrapper, *context, *layer_flow, *flow_context};
		SystemNodeExecute(context_wrapper);
	}

	

	bool LayerTaskFlow::AddTickedNode(SystemNode& node, Property property)
	{
		std::lock_guard lg(preprocess_mutex);

		auto task_node = Flow::AddNode_AssumedLocked(node, std::move(property.property));
		auto worst_node_index = worst_graph.Add();

		auto cur_mutex = node.GetMutex();

		bool accept = true;

		for (auto& ite : system_infos)
		{
			if (ite.node)
			{
				bool component_overlapping = false;
				bool singleton_overlapping = false;
				auto ite_mutex = ite.node->GetMutex();

				bool thread_order_overlapping = cur_mutex.thread_order_mark.WriteConfig(ite_mutex.thread_order_mark);

				if (!thread_order_overlapping)
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
						auto display_name = property.property.node_name;
						Order o1 = Order::UNDEFINE;
						Order o2 = Order::UNDEFINE;
						if (property.order_function != nullptr)
						{
							o1 = (*property.order_function)(display_name, ite.system_name);
						}
						if (ite.order_func != nullptr && ite.order_func != property.order_function)
						{
							o2 = (*ite.order_func)(ite.system_name, display_name);
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

					if (num_order == std::strong_ordering::equal)
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
					else if (num_order == std::strong_ordering::less)
					{
						if (need_worth_test && !worst_graph.AddEdge(worst_node_index, ite.worst_graph_node, { false }, Flow::config.temporary_resource))
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
					else if (num_order == std::strong_ordering::greater)
					{
						if (need_worth_test && !worst_graph.AddEdge(ite.worst_graph_node, worst_node_index, { false }, config.temporary_resource))
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
			worst_graph.RemoveNode(worst_node_index);
			Remove_AssumedLocked(task_node);
			return false;
		}else
		{
			if (task_node.GetIndex() < system_infos.size())
			{
				auto& ref = system_infos[task_node.GetIndex()];
				assert(!ref.node);
				ref.node = &node;
				ref.order_func = property.order_function;
				ref.priority = property.priority;
				ref.task_node = task_node;
				ref.worst_graph_node = worst_node_index;
				ref.system_name = property.property.node_name;
			}
			else
			{
				system_infos.emplace_back(
					&node,
					property.priority,
					property.order_function,
					task_node,
					worst_node_index,
					property.property.node_name
				);

				assert(system_infos.size() == task_node.GetIndex() + 1);
			}
		}
		return true;
	}

	/*
	bool LayerTaskFlow::AddTemporaryNodeImmediately(SystemNode& node, Potato::Task::Property property)
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
	*/

	void LayerTaskFlow::AddTemporaryNodeNextFrame(SystemNode& node, Potato::Task::Property property)
	{
		std::lock_guard lg(preprocess_mutex);
		defer_temporary_system_node.emplace_back(&node, std::move(property));
	}

	void LayerTaskFlow::PostUpdateFromFlow(Potato::TaskGraphic::FlowProcessContext& flow_context, Potato::Task::ContextWrapper& wrapper)
	{
		std::lock_guard lg(preprocess_mutex);
		if (!defer_temporary_system_node.empty())
		{
			Context* context = dynamic_cast<Context*>(wrapper.GetTriggerProperty().trigger.GetPointer());
			assert(context != nullptr);
			for (auto& ite : defer_temporary_system_node)
			{
				flow_context.AddTemporaryNode_AssumedLocked(*ite.node, [&](Potato::Task::Node const& t_node, Potato::Task::Property const& property, Potato::TaskGraphic::FlowNodeDetectionIndex const&)->bool
					{
						return LayerTaskFlow::TemporaryNodeDetect(*context, *ite.node, t_node, property);
					}, std::move(ite.property));
			}
			defer_temporary_system_node.clear();
		}
	}

	void LayerTaskFlow::AddTemporaryNode(Context& context, Potato::TaskGraphic::FlowProcessContext& flow_context, SystemNode& node, Potato::Task::Property property)
	{
		flow_context.AddTemporaryNode(node, [&](Potato::Task::Node const& t_node, Potato::Task::Property const& property, Potato::TaskGraphic::FlowNodeDetectionIndex const&)->bool
		{
			return LayerTaskFlow::TemporaryNodeDetect(context, node, t_node, property);
		}, std::move(property));
	}

	bool LayerTaskFlow::TemporaryNodeDetect(Context& context, SystemNode& node, Potato::Task::Node const& t_node, Potato::Task::Property const& property)
	{
		auto& source_node = dynamic_cast<SystemNode const&>(t_node);
		auto mutex1 = source_node.GetMutex();
		auto mutex2 = node.GetMutex();
		if (mutex1.thread_order_mark.WriteConfig(mutex2.thread_order_mark))
		{
			return true;
		}

		{
			std::shared_lock sl(context.singleton_manager.GetMutex());
			if (mutex1.singleton_mark.WriteConfigWithMask(mutex2.singleton_mark, context.singleton_manager.GetSingletonUsageMark_AssumedLocked()))
			{
				return true;
			}
		}

		{
			std::shared_lock sl(context.component_manager.GetMutex());
			if (mutex1.component_mark.WriteConfigWithMask(mutex2.component_mark, context.component_manager.GetArchetypeUsageMark_AssumedLocked()))
			{
				auto state = node.IsComponentOverlapping(source_node, context.component_manager.GetArchetypeUsageMark_AssumedLocked(), context.component_manager.GetArchetypeUsageMark_AssumedLocked());
				if (state == SystemNode::ComponentOverlappingState::IsOverlapped)
					return true;
			}
		}
		return false;
	}

	void LayerTaskFlow::TaskFlowExecuteBegin(Potato::Task::ContextWrapper& wrapper)
	{
		auto flow_context = wrapper.GetTaskNodeProperty().data.TryGetNodeDataPointerWithType<Potato::TaskGraphic::FlowProcessContext>();
		assert(flow_context != nullptr);
		auto context = dynamic_cast<Context*>(wrapper.GetTriggerProperty().trigger.GetPointer());
		assert(context != nullptr);

		std::lock_guard lg0(preprocess_mutex);
		std::shared_lock lg(context->component_manager.GetMutex());
		std::shared_lock lg2(context->singleton_manager.GetMutex());
		bool has_singleton_update = context->singleton_manager.HasSingletonUpdate_AssumedLocked();
		bool has_component_update = context->component_manager.HasArchetypeUpdate_AssumedLocked();

		if (has_singleton_update || has_component_update)
		{
			for (auto& ite : dynamic_edges)
			{
				auto from = static_cast<SystemNode*>(preprocess_nodes[ite.pre_process_from].node.GetPointer());
				auto to = static_cast<SystemNode*>(preprocess_nodes[ite.pre_process_to].node.GetPointer());
				bool need_edge = false;
				if (has_singleton_update && ite.singleton_overlapping)
				{
					auto mux1 = from->GetMutex();
					auto mux2 = to->GetMutex();

					if (
						mux1.singleton_mark.WriteConfigWithMask(mux2.singleton_mark, context->singleton_manager.GetSingletonUsageMark_AssumedLocked())
						)
					{
						need_edge = true;
					}
				}
				if (!need_edge && has_component_update && ite.component_overlapping)
				{
					auto state = from->IsComponentOverlapping(*to, context->component_manager.GetArchetypeUpdateMark_AssumedLocked(), context->component_manager.GetArchetypeUsageMark_AssumedLocked());
					switch (state)
					{
					case SystemNode::ComponentOverlappingState::NoUpdate:
						need_edge = ite.need_edge;
						break;
					case SystemNode::ComponentOverlappingState::IsNotOverlapped:
						need_edge = false;
						break;
					case SystemNode::ComponentOverlappingState::IsOverlapped:
						need_edge = true;
						break;
					}
				}
				if (need_edge && !ite.need_edge)
				{
					ite.need_edge = true;
					if (!ite.is_mutex)
					{
						AddDirectEdge_AssumedLocked(ite.pre_process_from, ite.pre_process_to, Potato::Graph::EdgeOptimize{ false });
					}
					else
					{
						AddMutexEdge_AssumedLocked(ite.pre_process_from, ite.pre_process_to, Potato::Graph::EdgeOptimize{ false });
					}
				}
				else if (!need_edge && ite.need_edge)
				{
					ite.need_edge = false;
					if (!ite.is_mutex)
					{
						RemoveDirectEdge_AssumedLocked(ite.pre_process_from, ite.pre_process_to);
					}
					else
					{
						RemoveMutexEdge_AssumedLocked(ite.pre_process_from, ite.pre_process_to);
					}
				}
			}
		}
	}

	/*
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
	*/

	/*
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
	*/

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
				LayerTaskFlow::Ptr ptr = new(re.Get()) LayerTaskFlow{ re, layer };
				auto cur_node = AddFlow_AssumedLocked(*ptr, { u8"sub_task" }, {});
				for (auto& ite : preprocess_nodes)
				{
					auto tar = static_cast<LayerTaskFlow*>(ite.node.GetPointer());
					if (tar != ptr)
					{
						if (tar->layout < layer)
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

	bool Context::AddTickedSystemNode(SystemNode& node, Property property)
	{
		std::lock_guard lg(preprocess_mutex);
		auto ptr = FindOrCreateContextTaskFlow_AssumedLocked(property.priority.layout);
		if (ptr)
		{
			return ptr->AddTickedNode(node, std::move(property));
		}
		return false;
	}

	bool Context::AddTemporarySystemNodeNextFrame(SystemNode& node, std::int32_t layout, Potato::Task::Property property)
	{
		std::lock_guard lg(preprocess_mutex);
		auto ptr = FindOrCreateContextTaskFlow_AssumedLocked(layout);
		if (ptr)
		{
			ptr->AddTemporaryNodeNextFrame(node, std::move(property));
			return true;
		}
		return false;
	}

	/*
	bool Context::Update_AssumedLocked(std::pmr::memory_resource* resource)
	{
		
	}
	*/

	void Context::TaskFlowExecuteBegin(Potato::Task::ContextWrapper& wrapper)
	{
		{
			std::lock_guard lg(mutex);
			auto cur = std::chrono::steady_clock::now();
			if (start_up_tick_lock.time_since_epoch().count() != 0)
				framed_duration = cur - start_up_tick_lock;
			start_up_tick_lock = cur;
		}
		component_manager.ClearArchetypeUpdateMark_AssumedLocked();
		entity_manager.Flush(component_manager);
		singleton_manager.Flush();
	}

	void Context::TaskFlowExecuteEnd(Potato::Task::ContextWrapper& wrapper)
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
		wrapper.GetTaskNodeProperty().data2.SetIndex(0);
		wrapper.Commit(*this, std::move(wrapper.GetTaskNodeProperty()), {}, require_time);
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
				auto loc = thread_order_manager.LocateOrAdd(*ite.struct_layout);
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

	struct ContextImplement : public Context, public Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
	public:
		ContextImplement(Potato::IR::MemoryResourceRecord record, Config fig) :Context(fig), MemoryResourceRecordIntrusiveInterface(record) {}
		virtual void AddContextRef() const override { MemoryResourceRecordIntrusiveInterface::AddRef(); }
		virtual void SubContextRef() const override { MemoryResourceRecordIntrusiveInterface::SubRef(); }
	};

	auto Context::Create(Config config, std::pmr::memory_resource* resource) -> Ptr
	{
		auto re = Potato::IR::MemoryResourceRecord::Allocate<Context>(resource);
		if (re)
		{
			return new (re.Get()) ContextImplement{re, std::move(config)};
		}
		return {};
	}


	/*
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
	*/
}