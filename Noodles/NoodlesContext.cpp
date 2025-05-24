module;

#include <cassert>

module NoodlesContext;



namespace Noodles
{
	using namespace Potato::Log;


	Instance::Instance(Config config, std::pmr::memory_resource* resource)
		: Executor(resource),
		component_map(config.component_class_count, resource),
		singleton_map(config.singleton_class_count, resource),
		exclusion_map(config.exclusion_class_count, resource),
		component_manager({ config.component_class_count, config.max_archetype_count }),
		entity_manager(component_map),
		singleton_manager(singleton_map.GetBitFlagContainerElementCount()),
		singleton_modify_manager(singleton_map.GetBitFlagContainerElementCount()),
		main_flow(resource),
		sub_flows(resource),
		system_info(resource)
	{

	}

	bool Instance::Commit(Potato::Task::Context& context, Parameter parameter)
	{
		Potato::TaskFlow::Executor::Parameter exe_parameter;
		exe_parameter.node_name = parameter.instance_name;
		exe_parameter.custom_data.data1 = parameter.duration_time.count();
		std::lock_guard lg(execute_state_mutex);
		if (execute_state == ExecuteState::State::Ready)
		{
			{
				std::lock_guard sl(flow_mutex);
				UpdateFlow_AssumedLocked(std::pmr::get_default_resource());
			}
			return Executor::Commit_AssumedLocked(context, exe_parameter);
		}
		return false;
	}

	struct InstanceImplement : public Instance, public Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		InstanceImplement(Potato::IR::MemoryResourceRecord record, Config config)
			: Instance(config, record.GetMemoryResource()), MemoryResourceRecordIntrusiveInterface(record)
		{

		}

	public:

		virtual void AddTaskFlowExecutorRef() const override { MemoryResourceRecordIntrusiveInterface::AddRef(); }
		virtual void SubTaskFlowExecutorRef() const override { MemoryResourceRecordIntrusiveInterface::SubRef(); }
		friend struct Ptr::CurrentWrapper;
		friend struct Ptr;
	};

	Instance::Ptr Instance::Create(Config config, std::pmr::memory_resource* resource)
	{
		auto re = Potato::IR::MemoryResourceRecord::Allocate<InstanceImplement>(resource);
		if (re)
		{
			return new(re.Get()) InstanceImplement{ re, config};
		}
		return {};
	}

	void Instance::BeginFlow(Potato::Task::Context& context, Potato::Task::Node::Parameter parameter)
	{
		{
			auto now = std::chrono::steady_clock::now();
			std::lock_guard lg(info_mutex);
			if (frame_count == 0)
			{
				delta_time = std::chrono::duration_cast<decltype(delta_time)::value_type>(std::chrono::microseconds{parameter.custom_data.data1});
			}
			else {
				delta_time = std::chrono::duration_cast<decltype(delta_time)::value_type>(now - startup_time);
			}
			++frame_count;
			startup_time = now;
		}
		Potato::Log::Log<InstanceLogCategory>(Potato::Log::Level::Log,
			L"Begin At[{:3}]({:.3f}) - {} In ThreadId<{}>", GetCurrentFrameCount() % 1000, GetDeltaTime().count(), parameter.node_name, std::this_thread::get_id()
		);

		bool component_modify = false;
		bool singleton_modify = false;
		{
			std::lock_guard lg(component_mutex);
			std::lock_guard lg2(entity_mutex);
			component_manager.ClearBitFlag();
			component_modify = entity_manager.FlushEntityModify(component_manager);
		}
		{
			std::lock_guard lg(singleton_mutex);
			std::lock_guard lg2(singleton_modify_mutex);
			singleton_modify = singleton_modify_manager.FlushSingletonModify(singleton_manager);
		}
		if (component_modify || singleton_modify)
		{

		}
	}

	void Instance::FinishFlow_AssumedLocked(Potato::Task::Context& context, Potato::Task::Node::Parameter parameter)
	{
		Executor::FinishFlow_AssumedLocked(context, parameter);

		{
			std::lock_guard lg(flow_mutex);
			UpdateFlow_AssumedLocked();
		}

		Executor::UpdateState_AssumedLocked();

		auto new_parameter = parameter;
		auto dur = std::chrono::milliseconds{ parameter.custom_data.data1 };
		{
			std::shared_lock sl(info_mutex);
			new_parameter.trigger_time = startup_time + dur;
		}
		auto re = Executor::Commit_AssumedLocked(context, new_parameter);
		assert(re);

		Potato::Log::Log<InstanceLogCategory>(Potato::Log::Level::Log,
			L"Finish At[{:3}]({:.3f}) - {} In ThreadId<{}>", GetCurrentFrameCount() % 1000, GetDeltaTime().count(), parameter.node_name, std::this_thread::get_id()
		);
	}

	bool Instance::AddSystemNode(SystemNode::Ptr node, SystemNode::Parameter parameter)
	{
		if(node)
		{
			std::lock_guard lg(flow_mutex);
			need_update = true;
			auto ite = std::find_if(sub_flows.begin(), sub_flows.end(), [&](SubFlowState& state) { return state.layer >= parameter.layer; });
			if(ite == sub_flows.end() || ite->layer != parameter.layer)
			{
				SubFlowState sub_flow{ parameter.layer, sub_flows.get_allocator().resource(), true, {} };
				ite = sub_flows.insert(ite, std::move(sub_flow));
			}
			assert(ite != sub_flows.end());

			std::size_t info_index = std::numeric_limits<std::size_t>::max();

			for (std::size_t i = 0; i < system_info.size(); ++i)
			{
				auto& info = system_info[i];
				if (!info.available)
				{
					info_index = i;
					break;
				}
			}

			if (info_index == std::numeric_limits<std::size_t>::max())
			{
				system_info.emplace_back();
				info_index = system_info.size() - 1;
			}

			Potato::TaskFlow::Node::Parameter t_paramter;
			t_paramter.node_name = parameter.system_name;
			t_paramter.custom_data.data1 = info_index;

			auto node_index = ite->flow.AddNode(*node, t_paramter);
			auto& info = system_info[info_index];
			info.available = true;
			info.index = node_index;
			info.parameter = parameter;

			return true;
		}
		return false;
	}

	bool Instance::UpdateFlow_AssumedLocked(std::pmr::memory_resource* resource)
	{
		if (need_update)
		{
			for (auto& ite : sub_flows)
			{
				if (ite.need_update)
				{
					main_flow.Remove(ite.index);
					ite.index = main_flow.AddFlowAsNode(ite.flow);
					ite.need_update = false;
					for (auto& ite2 : sub_flows)
					{
						if (ite.index != ite2.index)
						{
							assert(ite.layer != ite2.layer);
							if (ite.layer > ite2.layer)
							{
								main_flow.AddDirectEdge(ite.index, ite2.index);
							}
							else
							{
								main_flow.AddDirectEdge(ite2.index, ite.index);
							}
						}
					}
				}
			}
			need_update = false;
			UpdateFromFlow_AssumedLocked(main_flow, resource);
			return true;
		}
		return false;
	}

	void Instance::ExecuteNode(Potato::Task::Context& context, Potato::TaskFlow::Node& node, Potato::TaskFlow::Controller& controller)
	{
		Potato::Log::Log<InstanceLogCategory>(Potato::Log::Level::Log, L"start system {}", controller.GetParameter().node_name);
		Potato::Log::Log<InstanceLogCategory>(Potato::Log::Level::Log, L"finish system {}", controller.GetParameter().node_name);
	}


	/*
	void ContextWrapper::AddTemporarySystemNodeNextFrame(SystemNode& node, Potato::Task::Property property)
	{
		layer_flow.AddTemporaryNodeNextFrame(node, std::move(property));
	}

	void ContextWrapper::AddTemporarySystemNode(SystemNode& node, Potato::Task::Property property)
	{
		layer_flow.AddTemporaryNode(node, std::move(property));
	}

	void SystemNode::TaskGraphicNodeExecute(Potato::TaskGraphic::ContextWrapper& wrapper)
	{
		LayerTaskFlow& layer = static_cast<LayerTaskFlow&>(wrapper.GetFlow());
		Context& context = layer.GetContextFromTrigger();
		ContextWrapper context_wrapper{wrapper, context, layer };
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

	void LayerTaskFlow::AddTemporaryNodeNextFrame(SystemNode& node, Potato::Task::Property property)
	{
		std::lock_guard lg(preprocess_mutex);
		defer_temporary_system_node.emplace_back(&node, std::move(property));
	}

	void LayerTaskFlow::TaskFlowPostUpdateProcessNode_AssumedLocked(Potato::Task::ContextWrapper& wrapper)
	{
		if (!defer_temporary_system_node.empty())
		{
			Context* context = dynamic_cast<Context*>(wrapper.GetTriggerProperty().trigger.GetPointer());
			assert(context != nullptr);
			std::lock_guard lg(process_mutex);
			for (auto& ite : defer_temporary_system_node)
			{
				if (Flow::AddTemporaryNode_AssumedLocked(*ite.node, [&](Potato::TaskGraphic::Node& node, Potato::Task::Property& property, Potato::TaskGraphic::Node const& target_node, Potato::Task::Property const& target_property, Potato::TaskGraphic::FlowNodeDetectionIndex const&)->bool
					{
						return LayerTaskFlow::TemporaryNodeDetect(*context, static_cast<SystemNode&>(node), property, static_cast<SystemNode const&>(target_node), target_property);
					}, std::move(ite.property)))
				{
					ite.node->UpdateQuery(*context);
				}
			}
			defer_temporary_system_node.clear();
		}
	}

	bool LayerTaskFlow::AddTemporaryNode(SystemNode& node, Potato::Task::Property property)
	{
		std::lock_guard lg(process_mutex);
		return AddTemporaryNode_AssumedLocked(node, std::move(property));
	}

	bool LayerTaskFlow::AddTemporaryNode_AssumedLocked(SystemNode& node, Potato::Task::Property property)
	{
		if (Flow::AddTemporaryNode_AssumedLocked(node, [&](Potato::TaskGraphic::Node& node, Potato::Task::Property& property, Potato::TaskGraphic::Node const& target_node, Potato::Task::Property const& target_property, Potato::TaskGraphic::FlowNodeDetectionIndex const&)->bool
			{
				return LayerTaskFlow::TemporaryNodeDetect(GetContextFromTrigger(), static_cast<SystemNode&>(node), property, static_cast<SystemNode const&>(target_node), target_property);
			}, std::move(property)))
		{
			node.UpdateQuery(GetContextFromTrigger());
		}
		return false;
	}

	bool LayerTaskFlow::TemporaryNodeDetect(Context& context, SystemNode& node, Potato::Task::Property& property, SystemNode const& target_node, Potato::Task::Property const& target_property)
	{
		auto& source_node = static_cast<SystemNode const&>(target_node);
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

	Context& LayerTaskFlow::GetContextFromTrigger()
	{
		assert(trigger.trigger);
		auto context = static_cast<Context*>(trigger.trigger.GetPointer());
		return *context;
	}

	void LayerTaskFlow::TaskFlowExecuteBegin_AssumedLocked(Potato::Task::ContextWrapper& wrapper)
	{
		auto& context = *static_cast<Context*>(wrapper.GetTriggerProperty().trigger.GetPointer());
		std::shared_lock lg(context.component_manager.GetMutex());
		std::shared_lock lg2(context.singleton_manager.GetMutex());
		bool has_singleton_update = context.singleton_manager.HasSingletonUpdate_AssumedLocked();
		bool has_component_update = context.component_manager.HasArchetypeUpdate_AssumedLocked();

		bool new_system_node = false;
		for (auto& ite : preprocess_nodes)
		{
			if (ite.node)
			{
				if (!ite.under_process || has_singleton_update || has_component_update)
				{
					static_cast<SystemNode&>(*ite.node).UpdateQuery(context);
				}
				new_system_node = new_system_node || !ite.under_process;
			}
		}

		if (new_system_node || has_singleton_update || has_component_update)
		{
			for (auto& ite : dynamic_edges)
			{
				auto& from_ref = preprocess_nodes[ite.pre_process_from];
				auto& to_ref = preprocess_nodes[ite.pre_process_to];
				auto from = static_cast<SystemNode*>(from_ref.node.GetPointer());
				auto to = static_cast<SystemNode*>(to_ref.node.GetPointer());
				bool need_edge = false;
				if (has_singleton_update && ite.singleton_overlapping)
				{
					auto mux1 = from->GetMutex();
					auto mux2 = to->GetMutex();

					if (
						mux1.singleton_mark.WriteConfigWithMask(mux2.singleton_mark, context.singleton_manager.GetSingletonUsageMark_AssumedLocked())
						)
					{
						need_edge = true;
					}
				}
				if (!need_edge && has_component_update && ite.component_overlapping)
				{
					auto state = from->IsComponentOverlapping(*to, 
						(from_ref.under_process || to_ref.under_process) ? 
							context.component_manager.GetArchetypeUsageMark_AssumedLocked()
							:context.component_manager.GetArchetypeUpdateMark_AssumedLocked(),
						context.component_manager.GetArchetypeUsageMark_AssumedLocked()
					);
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

	void Context::Quit()
	{
		std::lock_guard lg(mutex);
		require_quit = true;
	}

	Context::Context(StructLayoutManager& manager, Config config)
		: manager(&manager), config(config), component_manager(manager), entity_manager(manager), singleton_manager(manager)
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
			auto re = Potato::IR::MemoryResourceRecord::Allocate<LayerTaskFlow>(std::pmr::get_default_resource());
			if (re)
			{
				LayerTaskFlow::Ptr ptr = new(re.Get()) LayerTaskFlow{ re, layer };
				auto cur_node = AddNode_AssumedLocked(*ptr, { u8"sub_task" });
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

	void Context::TaskFlowExecuteBegin_AssumedLocked(Potato::Task::ContextWrapper& wrapper)
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

	void Context::TaskFlowExecuteEnd_AssumedLocked(Potato::Task::ContextWrapper& wrapper)
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
		Commited_AssumedLocked(wrapper, std::move(wrapper.GetTaskNodeProperty()), {}, require_time);
	}

	struct ContextImplement : public Context, public Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
	public:
		ContextImplement(Potato::IR::MemoryResourceRecord record, StructLayoutManager& manager, Config fig) :Context(manager, fig), MemoryResourceRecordIntrusiveInterface(record) {}
		virtual void AddContextRef() const override { MemoryResourceRecordIntrusiveInterface::AddRef(); }
		virtual void SubContextRef() const override { MemoryResourceRecordIntrusiveInterface::SubRef(); }
	};

	auto Context::Create(StructLayoutManager& manager, Config config, std::pmr::memory_resource* resource) -> Ptr
	{
		auto re = Potato::IR::MemoryResourceRecord::Allocate<Context>(resource);
		if (re)
		{
			return new (re.Get()) ContextImplement{re, manager, std::move(config)};
		}
		return {};
	}
	*/
}