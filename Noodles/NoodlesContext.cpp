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

	SystemRequireBitFlagViewer Instance::GetSystemRequireBitFlagViewer_AssumedLocked(std::size_t system_info_index)
	{
		assert(system_info_index < system_info.size());
		auto commponent_count = component_map.GetBitFlagContainerElementCount();
		auto singleton_count = singleton_map.GetBitFlagContainerElementCount();
		auto total_count = commponent_count * 2 + singleton_count * 2;
		auto total_span = std::span(system_bitflag_container).subspan(
			total_count * system_info_index,
			total_count
		);

		SystemRequireBitFlagViewer result;

		result.component.require = BitFlagContainerViewer{ total_span.subspan(0, commponent_count) };
		total_span = total_span.subspan(commponent_count);
		result.component.write = total_span.subspan(0, commponent_count);
		total_span = total_span.subspan(commponent_count);

		result.singleton.require = total_span.subspan(0, singleton_count);
		total_span = total_span.subspan(singleton_count);
		result.singleton.write = total_span.subspan(0, singleton_count);
		total_span = total_span.subspan(singleton_count);

		return result;
	}

	Instance::SystemIndex Instance::PrepareSystemNode(SystemNode::Ptr node, SystemNode::Parameter parameter, SystemCategory category)
	{
		if (node)
		{
			SystemInitializer init{ *this };
			node->Init(init);

			std::lock_guard lg(system_mutex);

			auto finded = std::find_if(system_info.begin(), system_info.end(), [](SystemNodeInfo const& info) { return !info.node; });
			if (finded == system_info.end())
			{
				SystemNodeInfo node_info;
				node_info.component_query_index = { component_query.size(), component_query.size() };
				node_info.singleton_query_index = { singleton_query.size(), singleton_query.size() };
				auto total_bitflag_container_count = component_map.GetBitFlagContainerElementCount() * 2
					+ singleton_map.GetBitFlagContainerElementCount() * 2
					;
				auto old_size = system_bitflag_container.size();
				system_bitflag_container.resize(old_size + total_bitflag_container_count);
				BitFlagContainerViewer view{ std::span(system_bitflag_container).subspan(old_size) };
				view.Reset();
				finded = system_info.insert(finded, node_info);
			}

			finded->version += 1;
			finded->parameter = parameter;
			finded->node = std::move(node);
			finded->category = category;
			finded->flow_index = {};

			std::size_t index = static_cast<std::size_t>(finded - system_info.begin());

			bool has_new_query = false;

			auto system_bit_flag = GetSystemRequireBitFlagViewer_AssumedLocked(index);

			if (!init.component_list.empty())
			{
				has_new_query = true;

				for (auto& ite : init.component_list)
				{
					auto r1 = system_bit_flag.component.require.Union(ite->GetRequireContainerConstViewer());
					assert(r1);
					auto r2 = system_bit_flag.component.write.Union(ite->GetWritedContainerConstViewer());
					assert(r2);
				}

				finded->component_query_index.BackwardEnd(init.component_list.size());

				component_query.insert_range(
					component_query.begin() + finded->component_query_index.Begin(),
					std::ranges::as_rvalue_view(init.component_list)
				);
			}

			if (!init.singleton_list.empty())
			{
				has_new_query = true;

				for (auto& ite : init.singleton_list)
				{
					auto r1 = system_bit_flag.singleton.require.Union(ite->GetRequireContainerConstViewer());
					assert(r1);
					auto r2 = system_bit_flag.singleton.write.Union(ite->GetWritedContainerConstViewer());
					assert(r2);
				}

				finded->singleton_query_index.BackwardEnd(init.component_list.size());

				singleton_query.insert_range(
					singleton_query.begin() + finded->singleton_query_index.Begin(),
					std::ranges::as_rvalue_view(init.singleton_list)
				);
			}

			if (has_new_query)
			{
				for (auto ite = finded + 1; ite != system_info.end(); ++ite)
				{
					ite->component_query_index.WholeOffset(init.component_list.size());
					ite->singleton_query_index.WholeOffset(init.singleton_list.size());
				}
			}

			SystemIndex system_index = { index, finded->version };

			return system_index;
		}
		return {};
	}

	bool Instance::LoadSystemNode(SystemIndex index, std::size_t startup_system_index)
	{
		std::lock_guard sl(system_mutex);
		if (index && index.index < system_info.size())
		{
			auto& ref = system_info[index.index];
			if (index.version == ref.version && ref.node && !ref.flow_index)
			{
				if (ref.category == SystemCategory::Tick)
				{
					std::lock_guard lg(flow_mutex);
					flow_need_update = true;
					auto ite = std::find_if(sub_flows.begin(), sub_flows.end(), [&](SubFlowState& state) { return state.layer >= ref.parameter.layer; });
					if (ite == sub_flows.end() || ite->layer != ref.parameter.layer)
					{
						SubFlowState sub_flow{ ref.parameter.layer, sub_flows.get_allocator().resource(), true, {} };
						ite = sub_flows.insert(ite, std::move(sub_flow));
					}
					assert(ite != sub_flows.end());

					std::size_t sub_flow_index = static_cast<std::size_t>(ite - sub_flows.begin());

					Potato::TaskFlow::Node::Parameter t_paramter;
					t_paramter.node_name = ref.parameter.name;
					t_paramter.custom_data.data1 = index.index;
					t_paramter.custom_data.data2 = sub_flow_index;

					auto flow_index = ite->flow.AddNode(*ref.node, t_paramter);

					auto current_bitflag = GetSystemRequireBitFlagViewer_AssumedLocked(index.index);

					std::size_t ite_index = 0;
					for (auto& ite_sys : system_info)
					{
						if (ite_sys.category == SystemCategory::Tick && ite_sys.flow_index && ite_sys.parameter.layer == ref.parameter.layer)
						{
							auto ite_bitflag = GetSystemRequireBitFlagViewer_AssumedLocked(ite_index);

							if (
								*ite_bitflag.singleton.require.IsOverlapping(current_bitflag.singleton.write)
								|| *current_bitflag.singleton.require.IsOverlapping(ite_bitflag.singleton.write)
								)
							{
								auto result = (ite_sys.parameter.priority <=> ref.parameter.priority);
								if (result == std::strong_ordering::equal)
								{
									ite->flow.AddMutexEdge(ite_sys.flow_index, flow_index);
								}
								else if (result == std::strong_ordering::less)
								{
									ite->flow.AddDirectEdge(ite_sys.flow_index, flow_index);
								}
								else if (result == std::strong_ordering::greater)
								{
									ite->flow.AddDirectEdge(flow_index, ite_sys.flow_index);
								}
							}
						}
						ite_index += 1;
					}

					ref.flow_index = flow_index;

					return true;
				}
				else if (
					ref.category == SystemCategory::Template
					|| ref.category == SystemCategory::HighFrequencyTemplate
					)
				{
					Potato::TaskFlow::Node::Parameter t_paramter;
					t_paramter.node_name = ref.parameter.name;
					t_paramter.custom_data.data1 = index.index;
					t_paramter.custom_data.data2 = std::numeric_limits<std::size_t>::max();
					auto cur_bitflag = GetSystemRequireBitFlagViewer_AssumedLocked(index.index);

					/*
					Executor::AddTemplateNode(*ref.node, [&](Potato::TaskFlow::Sequencer& sequencer) -> bool {
						// todo
						return true;
						}, t_paramter, startup_system_index);
					*/

					return false;
				}
				else
				{
					assert(false);
				}
			}
		}
		return false;
	}

	bool Instance::UpdateFlow_AssumedLocked(std::pmr::memory_resource* resource)
	{
		if (flow_need_update)
		{
			std::size_t sub_flow_index = 0;
			for (auto& ite : sub_flows)
			{
				if (ite.need_update)
				{
					main_flow.Remove(ite.index);
					Potato::TaskFlow::Node::Parameter parameter;
					parameter.custom_data.data1 = std::numeric_limits<std::size_t>::max();
					parameter.custom_data.data2 = sub_flow_index;
					ite.index = main_flow.AddFlowAsNode(ite.flow, {}, parameter);
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
				sub_flow_index += 1;
			}
			flow_need_update = false;
			UpdateFromFlow_AssumedLocked(main_flow, resource);
			return true;
		}
		return false;
	}

	void Instance::ExecuteNode(Potato::Task::Context& context, Potato::TaskFlow::Node& node, Potato::TaskFlow::Controller& controller)
	{
		Potato::Log::Log<InstanceLogCategory>(Potato::Log::Level::Log, L"start system {}", controller.GetParameter().node_name);
		SystemNode* sys_node = static_cast<SystemNode*>(&node);
		Context instance_context{context, controller, *this, controller.GetParameter().custom_data.data1};
		sys_node->SystemNodeExecute(instance_context);
		Potato::Log::Log<InstanceLogCategory>(Potato::Log::Level::Log, L"finish system {}", controller.GetParameter().node_name);
	}

	std::tuple<std::size_t, std::size_t> Instance::GetQuery(std::size_t system_index, std::span<ComponentQuery::OPtr> out_component_query, std::span<SingletonQuery::OPtr> out_singleton_query)
	{
		std::shared_lock sl(system_mutex);
		if (system_index < system_info.size())
		{
			auto& ref = system_info[system_index];
			if (ref.node)
			{
				auto com_span = ref.component_query_index.Slice(std::span(component_query));
				auto sin_span = ref.singleton_query_index.Slice(std::span(singleton_query));
				std::size_t min_comp_size = std::min(out_component_query.size(), com_span.size());
				for (auto i = 0; i < min_comp_size; ++i)
				{
					out_component_query[i] = com_span[i];
				}
				std::size_t min_sing_size = std::min(out_singleton_query.size(), com_span.size());
				for (auto i = 0; i < min_sing_size; ++i)
				{
					out_singleton_query[i] = sin_span[i];
				}
				return {min_comp_size, min_sing_size};
			}
		}
		return {0, 0};
	}

	bool SingletonQueryInitializer::SetRequire(Potato::IR::StructLayout::Ptr struct_layout, bool is_writed)
	{
		if (iterator_index < require.size() && struct_layout)
		{
			auto re = bitflag_map.LocateOrAdd(*struct_layout);
			if (re)
			{
				require[iterator_index++] = *re;
				if (is_writed)
				{
					auto re2 = writed.SetValue(*re);
					if (re2)
					{
						return true;
					}
				}
			}
		}
		return false;
	}

	bool ComponentQueryInitializer::SetRefuse(Potato::IR::StructLayout::Ptr struct_layout)
	{
		if (struct_layout)
		{
			auto re = bitflag_map.LocateOrAdd(*struct_layout);
			if (re)
			{
				auto re2 = writed.SetValue(*re);
				if (re2)
				{
					return true;
				}
			}
		}
		return false;
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