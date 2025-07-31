module;

#include <cassert>

module NoodlesContext;



namespace Noodles
{
	using namespace Potato::Log;

	struct SubFlowSystemNode : public SystemNode
	{
		virtual void AddSystemNodeRef() const {}
		virtual void SubSystemNodeRef() const {}
		void SystemNodeExecute(Context& context)
		{
			if (context.controller.GetCategory() == Potato::TaskFlow::EncodedFlow::Category::SubFlowBegin)
			{
				auto param = context.controller.GetParameter();
				auto& instance = context.instance;
				std::int32_t layer = static_cast<std::int32_t>(param.custom_data.data2);
				std::lock_guard lg(instance.once_system_mutex);
				instance.current_layer = layer;

				while (instance.current_frame_once_system_iterator < instance.current_frame_once_system_count)
				{
					auto cur = instance.once_system_node[instance.current_frame_once_system_iterator];
					if (cur.parameter.layer <= layer)
					{
						++instance.current_frame_once_system_iterator;
						instance.LoadSystemNode(context, SystemCategory::OnceIgnoreLayer, cur.index, cur.parameter);
					}
					else {
						break;
					}
				}
			}
		}
	}sub_flow_system_node;

	struct EndingSystemNode : public SystemNode
	{
		virtual void AddSystemNodeRef() const {}
		virtual void SubSystemNodeRef() const {}
		void SystemNodeExecute(Context& context)
		{
			auto& instance = context.instance;
			if (!instance.available)
			{

			}
		}
	}ending_system_node;

	struct DyingSystemNode : public SystemNode
	{
		virtual void AddSystemNodeRef() const {}
		virtual void SubSystemNodeRef() const {}
		void SystemNodeExecute(Context& context)
		{
			auto& instance = context.instance;
			std::lock_guard lg(instance.once_system_mutex);
			instance.current_layer = std::numeric_limits<std::int32_t>::max();

			while (instance.current_frame_once_system_iterator < instance.current_frame_once_system_count)
			{
				auto cur = instance.once_system_node[instance.current_frame_once_system_iterator];
				++instance.current_frame_once_system_iterator;
				instance.LoadSystemNode(context, SystemCategory::OnceIgnoreLayer, cur.index, cur.parameter);
			}
		}
	}static_dying_system_node;

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
		system_info(resource),
		entity_history(component_map, resource)
	{
		Potato::TaskFlow::Node::Parameter parameter;
		parameter.node_name = L"EndingSystemNode";
		parameter.custom_data.data1 = std::numeric_limits<std::size_t>::max();
		ending_system_index = main_flow.AddNode(ending_system_node, parameter);
		parameter.node_name = L"DyingSystemNode";
		dying_system_index = main_flow.AddNode(static_dying_system_node, parameter);
		main_flow.AddDirectEdge(ending_system_index, dying_system_index);
	}

	bool Instance::Commit(Potato::Task::Context& context, Parameter parameter)
	{
		Potato::TaskFlow::Executor::Parameter exe_parameter;
		exe_parameter.node_name = parameter.instance_name;
		exe_parameter.custom_data.data1 = parameter.duration_time.count();
		std::lock_guard lg(execute_state_mutex);
		if (execute_state == ExecuteState::State::Ready)
		{
			UpdateSystems();
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
	}

	void Instance::FinishFlow_AssumedLocked(Potato::Task::Context& context, Potato::Task::Node::Parameter parameter)
	{
		Executor::FinishFlow_AssumedLocked(context, parameter);

		UpdateSystems();

		Executor::UpdateState_AssumedLocked();

		auto new_parameter = parameter;
		auto dur = std::chrono::milliseconds{ parameter.custom_data.data1 };
		{
			std::shared_lock sl(info_mutex);
			new_parameter.trigger_time = startup_time + dur;
		}
		

		Potato::Log::Log<InstanceLogCategory>(Potato::Log::Level::Log,
			L"Finish At[{:3}]({:.3f}) - {} In ThreadId<{}>", GetCurrentFrameCount() % 1000, GetDeltaTime().count(), parameter.node_name, std::this_thread::get_id()
		);

		if (available)
		{
			auto re = Executor::Commit_AssumedLocked(context, new_parameter);
			assert(re);
		}
	}

	bool Instance::DetectSystemComponentOverlapping_AssumedLocked(std::size_t system_index1, std::size_t system_index2)
	{
		auto& ref1 = system_info[system_index1];
		auto& ref2 = system_info[system_index2];

		auto viewer1 = GetSystemRequireBitFlagViewer_AssumedLocked(system_index1);
		auto viewer2 = GetSystemRequireBitFlagViewer_AssumedLocked(system_index2);

		if (*viewer1.archetype_usage.IsOverlapping(viewer2.archetype_usage))
		{
			std::span<ComponentQuery::Ptr> comp1 = ref1.component_query_index.Slice(std::span(component_query));
			std::span<ComponentQuery::Ptr> comp2 = ref2.component_query_index.Slice(std::span(component_query));

			for (auto& ite1 : comp1)
			{
				for (auto& ite2 : comp2)
				{
					if (*ite1->GetArchetypeContainerConstViewer().IsOverlappingWithMask(ite2->GetArchetypeContainerConstViewer(), component_manager.GetArchetypeNotEmptyBitFlag()))
					{
						if (
							*ite1->GetWritedContainerConstViewer().IsOverlapping(ite2->GetRequireContainerConstViewer())
							|| *ite2->GetWritedContainerConstViewer().IsOverlapping(ite1->GetRequireContainerConstViewer())
							)
						{
							return true;
						}
					}
				}
			}
		}
		return false;
	}

	SystemRequireBitFlagViewer Instance::GetSystemRequireBitFlagViewer_AssumedLocked(std::size_t system_info_index)
	{
		assert(system_info_index < system_info.size());
		auto commponent_count = component_map.GetBitFlagContainerElementCount();
		auto singleton_count = singleton_map.GetBitFlagContainerElementCount();
		auto archtype_count = component_manager.GetArchetypeBitFlagContainerCount();
		auto total_count = commponent_count * 2 + singleton_count * 2 + archtype_count;
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

		result.archetype_usage = total_span;
		total_span = total_span.subspan(archtype_count);

		return result;
	}

	std::optional<Instance::ExecutedSystemIndex> Instance::LoadOnceSystemNode(SystemNode::Ptr node, SystemNode::Parameter parameter)
	{
		SystemInfo info;
		info.temporary = true;
		auto index = PrepareSystemNode(node, info);
		if (index)
		{
			return LoadSystemNode(SystemCategory::OnceNextFrame, index, std::move(parameter));
		}
		return std::nullopt;
	}

	std::optional<Instance::ExecutedSystemIndex> Instance::LoadOnceSystemNode(Context& context, SystemCategory category, SystemNode::Ptr node, SystemNode::Parameter parameter)
	{
		if (category == SystemCategory::Tick)
			return std::nullopt;
		SystemInfo info;
		info.temporary = true;
		auto index = PrepareSystemNode(node, info);
		if (index)
		{
			return LoadSystemNode(context, category, index, std::move(parameter));
		}
		return std::nullopt;
	}


	Instance::SystemIndex Instance::PrepareSystemNode(SystemNode::Ptr node, SystemInfo info, bool unique)
	{
		if (node)
		{
			SystemInitializer init{ *this };
			InitSystemInitializer(init, *node);
			return PrepareSystemNode(init, std::move(node), info, unique);
		}
		return {};
	}

	void Instance::InitSystemInitializer(SystemInitializer& output, SystemNode& target_system) const
	{
		target_system.Init(output);
		{
			std::shared_lock sl(component_mutex);
			for (auto& ite : output.component_list)
			{
				ite->UpdateQueryData(component_manager);
			}
		}

		{
			std::shared_lock sl(singleton_mutex);
			for (auto& ite : output.singleton_list)
			{
				ite->UpdateQueryData(singleton_manager);
			}
		}
	}

	Instance::SystemIndex Instance::PrepareSystemNode(SystemInitializer& init, SystemNode::Ptr node, SystemInfo info, bool unique)
	{
		assert(node);
		std::lock_guard lg(system_mutex);
		if (unique && !info.identity_name.empty())
		{
			auto finded = std::find_if(system_info.begin(), system_info.end(), [&](SystemNodeInfo const& finded_info) {
				return finded_info.info.identity_name == info.identity_name;
				});
			if (finded != system_info.end())
			{
				return SystemIndex{
					static_cast<std::size_t>(std::distance(finded, system_info.begin())),
					finded->version
				};
			}
		}

		auto finded = std::find_if(system_info.begin(), system_info.end(), [](SystemNodeInfo const& info) { return !info.node; });
		if (finded == system_info.end())
		{
			SystemNodeInfo added_system_info;

			added_system_info.component_query_index = { component_query.size(), component_query.size() };
			added_system_info.singleton_query_index = { singleton_query.size(), singleton_query.size() };
			auto total_bitflag_container_count = component_map.GetBitFlagContainerElementCount() * 2
				+ singleton_map.GetBitFlagContainerElementCount() * 2
				+ component_manager.GetArchetypeBitFlagContainerCount()
				;
			auto old_size = system_bitflag_container.size();
			system_bitflag_container.resize(old_size + total_bitflag_container_count);
			BitFlagContainerViewer view{ std::span(system_bitflag_container).subspan(old_size) };
			view.Reset();
			finded = system_info.insert(system_info.end(), added_system_info);
		}
		else {
			finded->component_query_index = { component_query.size(), component_query.size() };
			finded->singleton_query_index = { singleton_query.size(), singleton_query.size() };
		}

		finded->version += 1;
		finded->node = std::move(node);
		finded->info = info;

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
				system_bit_flag.archetype_usage.Union(ite->GetArchetypeContainerConstViewer());
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

			finded->singleton_query_index.BackwardEnd(init.singleton_list.size());

			singleton_query.insert_range(
				singleton_query.begin() + finded->singleton_query_index.Begin(),
				std::ranges::as_rvalue_view(init.singleton_list)
			);
		}

		SystemIndex system_index = { index, finded->version };

		return system_index;
	}

	Instance::ExecuteSystemIndex Instance::FindAvailableSystemIndex_AssumedLocked()
	{
		auto finded = std::find_if(
			execute_system_info.begin(),
			execute_system_info.end(),
			[](ExecuteSystemNodeInfo const& ref) { return !ref.category.has_value();  }
		);

		if (finded == execute_system_info.end())
		{
			ExecuteSystemNodeInfo sys_info;
			finded = execute_system_info.insert(execute_system_info.end(), sys_info);
		}

		assert(finded != execute_system_info.end());
		
		return { static_cast<std::size_t>(finded - execute_system_info.begin()), finded->version };
	}

	std::optional<Instance::ExecutedSystemIndex> Instance::LoadSystemNode(SystemCategory category, SystemIndex index, SystemNode::Parameter parameter)
	{
		std::shared_lock sl(system_mutex);
		if (index && index.index < system_info.size())
		{
			auto& target_system = system_info[index.index];
			if (target_system.version == index.version)
			{
				switch (category)
				{
				case Noodles::SystemCategory::Tick:
				{
					std::lock_guard lg(execute_system_mutex);
					auto execute_sys_index = FindAvailableSystemIndex_AssumedLocked();

					assert(execute_sys_index);

					auto& ref = execute_system_info[execute_sys_index.index];

					std::lock_guard lg2(flow_mutex);
					flow_need_update = true;
					auto ite = std::find_if(sub_flows.begin(), sub_flows.end(), 
						[&](SubFlowState& state) { 
							return state.layer >= parameter.layer; 
						}
					);
					if (ite == sub_flows.end() || ite->layer != parameter.layer)
					{
						SubFlowState sub_flow{ parameter.layer, sub_flows.get_allocator().resource(), true, {} };
						ite = sub_flows.insert(ite, std::move(sub_flow));
					}
					assert(ite != sub_flows.end());

					Potato::TaskFlow::Node::Parameter t_paramter;
					t_paramter.node_name = parameter.name;
					t_paramter.custom_data.data1 = execute_sys_index.index;
					t_paramter.custom_data.data2 = index.index;

					auto flow_index = ite->flow.AddNode(*target_system.node, t_paramter);
					ite->need_update = true;
					assert(flow_index);

					ref.flow_index = flow_index;
					ref.sub_flow_index = static_cast<std::size_t>(ite - sub_flows.begin());
					ref.category = Noodles::SystemCategory::Tick;
					ref.parameter = std::move(parameter);
					ref.system_info_index = index.index;

					auto current_bitflag = GetSystemRequireBitFlagViewer_AssumedLocked(index.index);

					std::size_t ite_index = 0;
					for (auto& ite_sys : execute_system_info)
					{
						if (ite_sys.category.has_value() && ite_sys.category == SystemCategory::Tick && ite_sys.parameter.layer == parameter.layer && ite_index != execute_sys_index.index)
						{
							auto ite_bitflag = GetSystemRequireBitFlagViewer_AssumedLocked(ite_sys.system_info_index);

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
							else if (
									*ite_bitflag.component.require.IsOverlapping(current_bitflag.component.write)
									|| *current_bitflag.component.require.IsOverlapping(ite_bitflag.component.write)
									)
							{
								ref.has_dynamic_edges = true;
								auto result = (ite_sys.parameter.priority <=> ref.parameter.priority);

								bool has_edges = false;
								{
									std::shared_lock sl(component_mutex);
									has_edges = DetectSystemComponentOverlapping_AssumedLocked(index.index, ite_sys.system_info_index);
								}

								if (result == std::strong_ordering::equal)
								{
									if (has_edges)
									{
										ite->flow.AddDirectEdge(
											ite_sys.flow_index,
											flow_index
										);
									}

									dynamic_edges.emplace_back(
										ite_sys.system_info_index, 
										ite_index, index.index, execute_sys_index.index, false, has_edges
									);
								}
								else if (result == std::strong_ordering::less)
								{
									if (has_edges)
									{
										ite->flow.AddDirectEdge(
											flow_index,
											ite_sys.flow_index
										);
									}

									dynamic_edges.emplace_back(
										index.index,
										execute_sys_index.index, 
										ite_sys.system_info_index, ite_index, false, has_edges
									);
								}
								else if (result == std::strong_ordering::greater)
								{
									if (has_edges)
									{
										ite->flow.AddMutexEdge(
											flow_index,
											ite_sys.flow_index
										);
									}

									dynamic_edges.emplace_back(
										index.index,
										execute_sys_index.index, ite_sys.system_info_index, ite_index, true, has_edges
									);
								}
							}
						}
						ite_index += 1;					
					}
					return execute_sys_index;
				}
					break;
				case Noodles::SystemCategory::OnceNextFrame:
				{
					std::lock_guard lg(once_system_mutex);
					OnceSystemInfo once_sys_info;
					once_sys_info.parameter = parameter;
					once_sys_info.index = index;
					once_system_node.push_back(once_sys_info);
					return ExecutedSystemIndex{};
				}
					break;
				default:
					return std::nullopt;
					break;
				}
			}
		}
		return std::nullopt;
	}

	std::optional<Instance::ExecutedSystemIndex> Instance::LoadSystemNode(Context& context, SystemCategory category, SystemIndex index, SystemNode::Parameter parameter)
	{
		switch (category)
		{
		case SystemCategory::Tick:
		case SystemCategory::OnceNextFrame:
			return LoadSystemNode(category, index, std::move(parameter));
		case SystemCategory::Once:
		{
			{
				std::lock_guard sl(once_system_mutex);
				if (parameter.layer > current_layer)
				{
					auto finded = std::find_if(
						once_system_node.begin() + current_frame_once_system_iterator, 
						once_system_node.begin() + current_frame_once_system_count,
						[&](OnceSystemInfo const& info) {
							auto re = parameter.layer <=> info.parameter.layer;
							if (re == std::strong_ordering::equal)
							{
								re = parameter.priority <=> info.parameter.priority;
							}
							return re == std::strong_ordering::greater;
						}
						);
					once_system_node.insert(finded, OnceSystemInfo{std::move(parameter), index });
					++current_frame_once_system_count;
					return ExecuteSystemIndex{};
				}
			}
		}
		case SystemCategory::OnceIgnoreLayer:
		{
			std::shared_lock sl(system_mutex);
			if (index.index < system_info.size())
			{
				auto& ref = system_info[index.index];
				if (ref.version == index.version)
				{
					std::lock_guard lg(execute_system_mutex);
					
					auto exe_index = FindAvailableSystemIndex_AssumedLocked();
					auto& exe_ref = execute_system_info[exe_index.index];
					exe_ref.category = SystemCategory::Once;
					exe_ref.parameter = parameter;
					exe_ref.system_info_index = index.index;

					Potato::TaskFlow::Node::Parameter t_paramter;
					t_paramter.custom_data.data1 = exe_index.index;
					t_paramter.node_name = parameter.name;
					t_paramter.custom_data.data2 = index.index;
					t_paramter.acceptable_mask = parameter.acceptable_mask;
					
					std::shared_lock sl2(component_mutex);
					auto current_view = GetSystemRequireBitFlagViewer_AssumedLocked(index.index);
					auto add_result = context.controller.AddTemporaryNode(
						context.context, *ref.node, [&](Potato::TaskFlow::Sequencer& sequencer) -> bool {

							if (sequencer.GetSearchDepth() >= 6)
								return true;

							auto parameter = sequencer.GetCurrentParameter();

							if (
								parameter.custom_data.data1 == std::numeric_limits<std::size_t>::max()
								|| parameter.custom_data.data1 == std::numeric_limits<std::size_t>::max() - 1
								)
							{
								return true;
							}

							auto tar_sys_index = sequencer.GetCurrentParameter().custom_data.data2;
							assert(tar_sys_index < system_info.size());
							auto target_view = GetSystemRequireBitFlagViewer_AssumedLocked(tar_sys_index);

							if (
								*target_view.singleton.require.IsOverlapping(current_view.singleton.write)
								|| *current_view.singleton.require.IsOverlapping(target_view.singleton.write)
								)
							{
								return true;
							}

							if (
								*target_view.component.require.IsOverlapping(current_view.component.write)
								|| *current_view.component.require.IsOverlapping(target_view.component.write)
								)
							{
								return DetectSystemComponentOverlapping_AssumedLocked(index.index, tar_sys_index);
							}

							return false;
						},
						t_paramter
					);
					has_template_system = true;
					return ExecutedSystemIndex{};
				}
			}
			break;
		}
		}
		return std::nullopt;
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
					parameter.node_name = L"SubFlowBoundary";
					parameter.custom_data.data1 = std::numeric_limits<std::size_t>::max() - 1;
					parameter.custom_data.data2 = static_cast<std::size_t>(ite.layer);
					ite.index = main_flow.AddFlowAsNode(ite.flow, &sub_flow_system_node, parameter);
					ite.need_update = false;
					for (auto& ite2 : sub_flows)
					{
						if (ite.index != ite2.index)
						{
							assert(ite.layer != ite2.layer);
							if (ite.layer < ite2.layer)
							{
								main_flow.AddDirectEdge(ite.index, ite2.index);
							}
							else
							{
								main_flow.AddDirectEdge(ite2.index, ite.index);
							}
						}
					}
					main_flow.AddDirectEdge(ite.index, ending_system_index);
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
		std::shared_lock sl(component_mutex);
		std::shared_lock s2(singleton_mutex);
		auto task_node_parameter = controller.GetParameter();
		if(controller.GetCategory() == Potato::TaskFlow::EncodedFlow::Category::SubFlowBegin)
			Potato::Log::Log<InstanceLogCategory>(Potato::Log::Level::Log, L"SubFlow Begin {}", task_node_parameter.node_name);
		else if(controller.GetCategory() != Potato::TaskFlow::EncodedFlow::Category::SubFlowEnd)
			Potato::Log::Log<InstanceLogCategory>(Potato::Log::Level::Log, L"+--Execute Begin System {}", controller.GetParameter().node_name);
		SystemNode* sys_node = static_cast<SystemNode*>(&node);
		Context instance_context{context, controller, *this, task_node_parameter.custom_data.data2};
		sys_node->SystemNodeExecute(instance_context);
		if (controller.GetCategory() == Potato::TaskFlow::EncodedFlow::Category::SubFlowEnd)
		{
			Potato::Log::Log<InstanceLogCategory>(Potato::Log::Level::Log, L"SubFlow End {}", task_node_parameter.node_name);
		}
		else if (controller.GetCategory() != Potato::TaskFlow::EncodedFlow::Category::SubFlowBegin)
		{
			Potato::Log::Log<InstanceLogCategory>(Potato::Log::Level::Log, L"+--Execute End System {}", task_node_parameter.node_name);
		}
	}

	void Instance::UpdateSystems()
	{

		bool component_has_modify = false;
		{
			std::lock(component_mutex, entity_mutex);
			std::lock_guard sl(component_mutex, std::adopt_lock);
			std::lock_guard s2(entity_mutex, std::adopt_lock);

			entity_manager.FlushEntityModify(component_manager, entity_history);

			component_has_modify = component_manager.HasArchetypeCreated();
		}

		bool singleton_has_modify = false;
		{
			std::lock(singleton_mutex, singleton_modify_mutex);
			std::lock_guard sl(singleton_mutex, std::adopt_lock);
			std::lock_guard s2(singleton_modify_mutex, std::adopt_lock);

			singleton_has_modify = singleton_modify_manager.FlushSingletonModify(singleton_manager);
		}

		{
			std::lock(system_mutex, execute_system_mutex, flow_mutex, once_system_mutex);

			std::lock_guard sl(system_mutex, std::adopt_lock);
			std::lock_guard s2(execute_system_mutex, std::adopt_lock);
			std::lock_guard s3(flow_mutex, std::adopt_lock);
			std::lock_guard lg(once_system_mutex, std::adopt_lock);

			std::shared_lock sl4(component_mutex);
			std::shared_lock sl5(singleton_mutex);

			{
				assert(current_frame_once_system_iterator == current_frame_once_system_count);

				once_system_node.erase(
					once_system_node.begin(),
					once_system_node.begin() + current_frame_once_system_iterator
				);

				current_frame_once_system_iterator = 0;
				current_frame_once_system_count = once_system_node.size();

				std::sort(
					once_system_node.begin(),
					once_system_node.end(),
					[](OnceSystemInfo const& sys1, OnceSystemInfo const& sys2)
					{
						auto re = sys1.parameter.layer <=> sys2.parameter.layer;

						if (re == std::strong_ordering::equal)
						{
							re = sys1.parameter.priority <=> sys2.parameter.priority;
						}

						return re == std::strong_ordering::less;
					}
				);

				current_layer = std::numeric_limits<std::int32_t>::max();
			}

			if (has_template_system)
			{
				bool need_remove_temporary_system = false;
				has_template_system = false;
				for (auto& execute_system_ite : execute_system_info)
				{
					if (execute_system_ite.category.has_value() && execute_system_ite.category == SystemCategory::Once)
					{
						auto version = execute_system_ite.version;
						system_info[execute_system_ite.system_info_index].has_temporary_last_frame = true;
						if (system_info[execute_system_ite.system_info_index].info.temporary)
						{
							need_remove_temporary_system = true;
						}
						execute_system_ite = ExecuteSystemNodeInfo{};
						execute_system_ite.version = version + 1;
					}
				}
				if (need_remove_temporary_system)
				{
					for (auto& ite : once_system_node)
					{
						if (ite.index.index < system_info.size())
						{
							auto& ref = system_info[ite.index.index];
							if (ref.version == ref.version)
							{
								ref.has_temporary_last_frame = false;
							}
						}
					}

					for (auto ite = system_info.begin(); ite != system_info.end(); ++ite)
					{
						if (ite->node && ite->info.temporary && ite->has_temporary_last_frame)
						{
							auto comp_que_index = ite->component_query_index;
							auto sing_que_index = ite->singleton_query_index;
							bool need_fix_index = false;
							if (comp_que_index.Size() > 0)
							{
								need_fix_index = true;
								auto component_span = comp_que_index.Slice(std::span(component_query));
								for (auto& ite2 : component_span)
								{
									ite2.Reset();
								}
								component_query.erase(
									component_query.begin() + comp_que_index.Begin(),
									component_query.begin() + comp_que_index.End()
								);
							}

							if (sing_que_index.Size() > 0)
							{
								need_fix_index = true;
								auto singleton_span = sing_que_index.Slice(std::span(singleton_query));
								for (auto& ite2 : singleton_span)
								{
									ite2.Reset();
								}
								singleton_query.erase(
									singleton_query.begin() + sing_que_index.Begin(),
									singleton_query.begin() + sing_que_index.End()
								);
							}

							ite->node.Reset();

							if (need_fix_index)
							{
								for (auto& ite2 : system_info)
								{
									if (ite2.node)
									{
										if (ite2.component_query_index.Begin() >= comp_que_index.End())
										{
											ite2.component_query_index.WholeForward(comp_que_index.Size());
										}
										if (ite2.singleton_query_index.Begin() >= sing_que_index.End())
										{
											ite2.singleton_query_index.WholeForward(sing_que_index.Size());
										}
									}
								}
							}
							auto version = ite->version;
							*ite = SystemNodeInfo{};
							ite->version = version + 1;
						}
					}
				}
			}

			if (component_has_modify || singleton_has_modify)
			{
				std::size_t system_info_index = 0;
				for (auto& ite : system_info)
				{
					if (ite.node)
					{
						auto Viewer = GetSystemRequireBitFlagViewer_AssumedLocked(system_info_index);
						if (component_has_modify)
						{
							auto span = ite.component_query_index.Slice(std::span(component_query));
							for (auto& ite2 : span)
							{
								if (ite2->UpdateQueryData(component_manager))
								{
									Viewer.archetype_usage.Union(ite2->GetArchetypeContainerConstViewer());
								}
							}
						}

						if (singleton_has_modify)
						{
							auto span = ite.singleton_query_index.Slice(std::span(singleton_query));
							for (auto& ite2 : span)
							{
								ite2->UpdateQueryData(singleton_manager);
							}
						}
					}
					system_info_index += 1;
				}
			}

			if (!component_manager.GetArchetypeUpdateBitFlag().IsReset())
			{
				for (auto& ite : dynamic_edges)
				{
					auto from_viewer = GetSystemRequireBitFlagViewer_AssumedLocked(ite.from_system_index);

					if (*from_viewer.archetype_usage.IsOverlapping(component_manager.GetArchetypeUpdateBitFlag()))
					{
						bool need_edge = DetectSystemComponentOverlapping_AssumedLocked(ite.from_system_index, ite.to_system_index);
						if (need_edge != ite.added)
						{
							ite.added = need_edge;

							auto& from_ref = execute_system_info[ite.from_execute_system_index];
							auto& to_ref = execute_system_info[ite.to_execute_system_index];

							auto& current_flow = sub_flows[from_ref.sub_flow_index];
							if (need_edge)
							{
								if (ite.is_mutex)
								{
									current_flow.flow.AddMutexEdge(
										from_ref.flow_index, to_ref.flow_index
									);
								}
								else {
									current_flow.flow.AddDirectEdge(
										from_ref.flow_index, to_ref.flow_index
									);
								}
							}
							else {
								if (ite.is_mutex)
								{
									current_flow.flow.RemoveMutexEdge(
										from_ref.flow_index, to_ref.flow_index
									);
								}
								else {
									current_flow.flow.RemoveDirectEdge(
										from_ref.flow_index, to_ref.flow_index
									);
								}
							}
							current_flow.need_update = true;
							flow_need_update = true;
						}
					}
				}
			}

			UpdateFlow_AssumedLocked();
		}
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
				std::size_t min_sing_size = std::min(out_singleton_query.size(), sin_span.size());
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
				auto re2 = refuse.SetValue(*re);
				if (re2)
				{
					return true;
				}
			}
		}
		return false;
	}

	SystemNode::Parameter Context::GetParameter() const
	{
		auto par = controller.GetParameter().custom_data;
		std::shared_lock sl(instance.execute_system_mutex);
		assert(par.data1 < instance.execute_system_info.size());
		return instance.execute_system_info[par.data1].parameter;
	}
}