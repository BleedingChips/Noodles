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

	static Potato::Format::StaticFormatPattern<u8"{}:{}"> system_static_format_pattern;

	Potato::IR::Layout SystemName::GetSerializeLayout() const
	{
		Potato::Format::FormatWritter<char8_t> temp_writer;
		auto re = system_static_format_pattern.Format(temp_writer, group, name);
		assert(re);
		return Potato::IR::Layout::GetArray<char8_t>(temp_writer.GetWritedSize());
	}

	void SystemName::SerializeTo(std::span<std::byte> output) const
	{
		Potato::Format::FormatWritter<char8_t> temp_writer{
			std::span<char8_t>{reinterpret_cast<char8_t*>(output.data()), output.size()}
		};
		auto re = system_static_format_pattern.Format(temp_writer, group, name);
		assert(re);
	}
	SystemName SystemName::ReMap(std::span<std::byte> input) const
	{
		return {
			std::u8string_view{reinterpret_cast<char8_t*>(input.data() + sizeof(char8_t) * ( group.size() + 1)), name.size()},
			std::u8string_view{reinterpret_cast<char8_t*>(input.data()), group.size()}
		};
	}

	bool LayerTaskFlow::AddTickedNode(SystemNode::Ptr node, SystemNodeProperty property)
	{
		if(node)
		{
			std::lock_guard lg(preprocess_mutex);

			std::size_t index = 0;

			for(;index < preprocess_system_infos.size(); ++index)
			{
				if(!preprocess_system_infos[index].node)
					break;
			}

			if(index == preprocess_system_infos.size())
				preprocess_system_infos.emplace_back();

			auto cur_node = pre_test_graph.Add(index);
			assert(cur_node);
			Potato::Task::TaskFlowNodeProperty nproperty
			{
				node->GetDisplayName().name,
				property.filter
			};
			auto pre_process_node = TaskFlow::AddNode_AssumedLocked(node.GetPointer(), nproperty, index);
			assert(pre_process_node);

			auto cur_mutex = node->GetMutex();

			bool accept = true;

			for(auto& ite : preprocess_system_infos)
			{
				if(ite.node)
				{
					EdgeProperty e_property;
					auto ite_mutex = ite.node->GetMutex();

					bool thread_order_overlapping =
						MarkElement::IsOverlapping(
							cur_mutex.thread_order_mark, ite_mutex.thread_order_write_mark
						)
						||
						MarkElement::IsOverlapping(
							cur_mutex.thread_order_write_mark, ite_mutex.thread_order_mark
						)
						;

					if(!thread_order_overlapping)
					{
						e_property.component_overlapping =
							MarkElement::IsOverlapping(
								cur_mutex.component_mark, ite_mutex.component_write_mark
							)
							||
							MarkElement::IsOverlapping(
								cur_mutex.component_write_mark, ite_mutex.component_mark
							)
							;
						e_property.singleton_overlapping =
							MarkElement::IsOverlapping(
								cur_mutex.singleton_mark, ite_mutex.singleton_write_mark
							)
							||
							MarkElement::IsOverlapping(
								cur_mutex.singleton_write_mark, ite_mutex.singleton_mark
							)
							;
					}

					if(thread_order_overlapping)
					{
						auto re = AddDirectEdge_AssumedLocked(
							pre_process_node, ite.process_graph_node, {false, true}
						);
						if(!re)
						{
							accept = false;
							break;
						}
					}else if(e_property.component_overlapping || e_property.singleton_overlapping)
					{
						auto num_order = property.priority <=> ite.priority;
						bool is_mutex = false;
						if (num_order == std::strong_ordering::equal)
						{
							auto disname = node->GetDisplayName();
							Order o1 = Order::UNDEFINE;
							Order o2 = Order::UNDEFINE;
							if (property.order_function != nullptr)
							{
								o1 = (*property.order_function)(disname, ite.name);
							}
							if (ite.order_func != nullptr && ite.order_func != property.order_function)
							{
								o2 = (*ite.order_func)(ite.name, disname);
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
								is_mutex = true;
							}
						}


						if (num_order == std::strong_ordering::greater)
						{
							auto re = pre_test_graph.AddEdge(cur_node, ite.pre_test_graph_node, { false, true });
							if (!re)
							{
								accept = false;
								break;
							}
						}
						else if (num_order == std::strong_ordering::less)
						{
							auto re = pre_test_graph.AddEdge(ite.pre_test_graph_node, cur_node, { false, true });
							if (!re)
							{
								accept = false;
								break;
							}
						}
						else
						{
							assert(is_mutex);
							TaskFlow::AddMutexEdge_AssumedLocked(pre_process_node, ite.process_graph_node, EdgeOptimize{ false, false });
						}
					}
				}
			}

			Potato::Task::TaskFlowNodeProperty nproperty
			{
				node->GetDisplayName().name,
				property.filter
			};

			if (AddNode_AssumedLocked(static_cast<Potato::Task::TaskFlowNode*>(node.GetPointer()), nproperty))
			{
				std::size_t index = 0;
				auto disname = node.GetDisplayName();
				for (auto& ite : preprocess_system_infos)
				{
					ReadWriteMutex ite_mutex{ std::span(preprocess_rw_id), ite.read_write_mutex };
					if (ite_mutex.IsConflict(mutex))
					{
						auto num_order = property.priority <=> ite.priority;
						if (num_order == std::strong_ordering::greater)
						{
							auto re = AddDirectEdge_AssumedLocked(node, *preprocess_nodes[index].node);
							if (!re)
							{
								gener.RecoverModify();
								Remove_AssumedLocked(node);
								return false;
							}
						}
						else if (num_order == std::strong_ordering::less)
						{
							auto re = AddDirectEdge_AssumedLocked(*preprocess_nodes[index].node, node);
							if (!re)
							{
								gener.RecoverModify();
								Remove_AssumedLocked(node);
								return false;
							}
						}
						else
						{
							Order o1 = Order::UNDEFINE;
							Order o2 = Order::UNDEFINE;
							if (property.order_function != nullptr)
							{
								o1 = (*property.order_function)(disname, ite.name);
							}
							if (ite.order_func != nullptr && ite.order_func != property.order_function)
							{
								o2 = (*ite.order_func)(ite.name, disname);
							}
							if (
								o1 == Order::BIGGER && o2 == Order::SMALLER
								|| o1 == Order::SMALLER && o2 == Order::BIGGER
								|| o1 == Order::UNDEFINE && o2 == Order::UNDEFINE
								)
							{
								gener.RecoverModify();
								Remove_AssumedLocked(node);
								return false;
							}
							else if (o1 == Order::UNDEFINE)
							{
								o1 = o2;
							}
							if (o1 == Order::SMALLER)
							{
								auto re = AddDirectEdge_AssumedLocked(*preprocess_nodes[index].node, node);
								if (!re)
								{
									gener.RecoverModify();
									Remove_AssumedLocked(node);
									return false;
								}
							}
							else if (o1 == Order::BIGGER)
							{
								auto re = AddDirectEdge_AssumedLocked(node, *preprocess_nodes[index].node);
								if (!re)
								{
									gener.RecoverModify();
									Remove_AssumedLocked(node);
									return false;
								}
							}
							else if (o1 == Order::MUTEX)
							{
								auto re = AddMutexEdge_AssumedLocked(node, *preprocess_nodes[index].node);
								if (!re)
								{
									gener.RecoverModify();
									Remove_AssumedLocked(node);
									return false;
								}
							}
						}
					}
					++index;
				}
				preprocess_system_infos.emplace_back(
					mutex.index,
					property.priority,
					property.order_function,
					disname
				);
				return true;
			}
			gener.RecoverModify();
		}
		return false;
	}

	bool LayerTaskFlow::AddTemporaryNodeImmediately(SystemNode::Ptr node, Potato::Task::TaskFilter filter)
	{
		std::lock_guard lg(process_mutex);
		return AddTemporaryNodeImmediately_AssumedLocked(node, filter);
	}

	bool LayerTaskFlow::AddTemporaryNodeImmediately_AssumedLocked(SystemNode::Ptr node, Potato::Task::TaskFilter filter)
	{
		if(finished_task >= process_nodes.size())
			return false;
		ReadWriteMutexGenerator ge(process_rw_id);
		auto s_node = static_cast<SystemNode const*>(&node);
		s_node->FlushMutexGenerator(ge);
		auto rw_mutex = ge.GetMutex();
		auto name = node.GetDisplayName();
		if(TaskFlow::AddTemporaryNode_AssumedLocked(
			node,
			{name.name, filter},
			[=, this](TaskFlowNode const& node, Potato::Task::TaskFlowNodeProperty property, TemporaryNodeIndex index)-> bool
			{
				assert(index.current_index < process_system_infos.size());
				ReadWriteMutex index_rw_mutex{
					std::span(process_rw_id),
					process_system_infos[index.current_index].read_write_mutex
				};
				return rw_mutex.IsConflict(index_rw_mutex);
			}
		))
		{
			process_system_infos.emplace_back(rw_mutex.index);
			return true;
		}else{
			ge.RecoverModify();
		}
		return false;
	}

	bool LayerTaskFlow::AddTemporaryNodeDefer(SystemNode& node, Potato::Task::TaskFilter filter)
	{
		std::lock_guard lg(process_mutex);
		defer_temporary_system_node.emplace_back(&node, filter);
		return true;
	}

	bool LayerTaskFlow::Update_AssumedLocked(std::pmr::memory_resource* resource)
	{
		bool par = need_update;
		if(TaskFlow::Update_AssumedLocked())
		{
			if(par)
			{
				process_rw_id = preprocess_rw_id;
				process_system_infos.clear();
				for(auto& ite : preprocess_system_infos)
				{
					process_system_infos.emplace_back(ite.read_write_mutex);
				}
				temporary_rw_id_offset = process_rw_id.size();
			}else
			{
				process_system_infos.resize(temporary_node_offset);
				process_rw_id.resize(temporary_rw_id_offset);
			}
			for(auto& ite : defer_temporary_system_node)
			{
				auto re = AddTemporaryNodeImmediately_AssumedLocked(*ite.ptr, ite.filter);
				assert(re);
			}
			defer_temporary_system_node.clear();
			return true;
		}
		for(auto& ite : defer_temporary_system_node)
		{
			auto re = AddTemporaryNodeImmediately_AssumedLocked(*ite.ptr, ite.filter);
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

	Context::Context(Config config, SyncResource resource)
		: config(config), 
		manager({
			resource.context_resource,
			resource.archetype_resource,
			resource.component_resource,
			resource.singleton_resource,
			resource.filter_resource,
			resource.temporary_resource
		}),
		system_resource(resource.system_resource),
		entity_resource(resource.entity_resource),
		temporary_resource(resource.temporary_resource),
		context_resource(resource.context_resource)
	{

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
			auto re = Potato::IR::MemoryResourceRecord::Allocate<SubContextTaskFlow>(context_resource);
			if (re)
			{
				LayerTaskFlow::Ptr ptr = new(re.Get()) LayerTaskFlow{ re, layer };
				AddNode_AssumedLocked(*ptr, { u8"sub_task" });
				for (auto& ite : preprocess_nodes)
				{
					auto tar = static_cast<LayerTaskFlow*>(ite.node.GetPointer());
					if (tar != ptr)
					{
						if (tar->layout > layer)
						{
							auto re = AddDirectEdge_AssumedLocked(*tar, *ptr);
							assert(re);
						}
						else
						{
							assert(tar->layout != layer);
							auto re = AddDirectEdge_AssumedLocked(*ptr, *tar);
							assert(re);
						}
					}
				}
				return ptr;
			}
		}
		
		return ptr;
	}

	bool Context::AddTickedSystemNode(SystemNode& node, SystemNodeProperty property)
	{
		std::lock_guard lg(preprocess_mutex);
		auto ptr = FindOrCreateContextTaskFlow_AssumedLocked(property.priority.layout);
		if(ptr)
		{
			return ptr->AddTickedNode(node, property);
		}
		return false;
	}

	bool Context::AddTemporarySystemNodeDefer(SystemNode& node, std::int32_t layout, Potato::Task::TaskFilter filter)
	{
		std::lock_guard lg(preprocess_mutex);
		auto ptr = FindOrCreateContextTaskFlow_AssumedLocked(layout);
		if (ptr)
		{
			return ptr->AddTemporaryNodeDefer(node, filter);
		}
		return false;
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

	bool Context::Commited(Potato::Task::TaskContext& context, Potato::Task::TaskFlowNodeProperty property)
	{
		std::lock_guard lg(TaskFlow::process_mutex);
		if(current_status == Status::DONE || current_status == Status::READY)
		{
			entity_manager.Flush(component_manager);
			singleton_manager.Flush();
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