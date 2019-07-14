#include "system_pool.h"
#include <algorithm>
#include <vector>

namespace Noodles::Error
{
	SystemOrderRecursion::SystemOrderRecursion(std::vector<const char*> inf, std::vector<const char*> ct, std::vector<std::array<size_t, 3>> cb)
		: std::logic_error("System Order Recursion"), infos(std::move(inf)), conflig_type(std::move(ct)), conflig_type_bound(std::move(cb)) {}

	SystemOrderConflig::SystemOrderConflig(const char* inf, const char* t, std::array<size_t, 3> cb, std::vector<const char*> ct)
		: std::logic_error("System Order Conflig"), si(inf), ti(t), conflig_bound(cb), conflig_type(std::move(ct)) {}
}


namespace Noodles::Implement
{
	void SystemPool::regedit_system(SystemInterface* in) noexcept
	{
		assert(in != nullptr);
		std::lock_guard lg(m_log_mutex);
		m_regedited_system.push_back(SystemInterfacePtr{in});
	}

	void SystemPool::destory_system(const TypeInfo& info) noexcept
	{
		std::lock_guard lg(m_log_mutex);
		m_regedited_system.push_back(info);
	}

	void SystemPool::regedit_template_system(SystemInterface* in) noexcept
	{
		std::lock_guard lg(m_log_mutex);
		m_regedited_temporary_system.push_back(in);
	}

	void SystemPool::handle_relationship_component_conflig(SystemRelationShip& relationship, size_t component_size)
	{
		if (relationship.conflig_bound[2] != 0)
		{
			std::vector<ReadWriteProperty> t1_buffer{ component_size, ReadWriteProperty::Unknow };
			std::vector<ReadWriteProperty> t2_buffer{ component_size, ReadWriteProperty::Unknow };
			const TypeInfo* ti_start = &relationship.conflig_type[relationship.conflig_bound[0] + relationship.conflig_bound[1]];
			size_t ti_length = relationship.conflig_bound[2];
			relationship.active->second.ptr->type_group_usage(ti_start, ti_length, t1_buffer.data());
			relationship.passtive->second.ptr->type_group_usage(ti_start, ti_length, t2_buffer.data());
			relationship.component_check_start = m_conflig_component_state.size();
			size_t count = 0;
			for (size_t index = 0; index < t1_buffer.size(); ++index)
			{
				auto t1_b = t1_buffer[index];
				auto t2_b = t2_buffer[index];
				if (
					(t1_b == ReadWriteProperty::Write && t2_b != ReadWriteProperty::Unknow)
					|| (t2_b == ReadWriteProperty::Write && t1_b != ReadWriteProperty::Unknow)
					)
				{
					m_conflig_component_state.push_back(index);
					++count;
				}
			}
			relationship.component_check_length = count;
		}
		
	}
	
	bool SystemPool::update(bool component_change, bool gobal_component_change, ComponentPool& cp, GobalComponentPool& gcp)
	{
		std::lock_guard lg(m_log_mutex);
		std::unique_lock up(m_systems_mutex);
		std::lock_guard lg2(m_state_mutex);
		cp.update_type_group_state(m_component_state);
		std::swap(m_regedited_temporary_system, m_using_temporary);
		m_regedited_temporary_system.clear();
		if (!m_regedited_system.empty())
		{
			if (component_change || gobal_component_change)
			{
				for (auto& ite : m_systems)
					ite.second.ptr->envirment_change(false, gobal_component_change, component_change);
				for (auto& ite2 : m_using_temporary)
					ite2->envirment_change(false, gobal_component_change, component_change);
			}
			for (auto& ite : m_regedited_system)
			{
				std::visit(Potato::Tool::overloaded{
					[&, this](SystemInterfacePtr& ptr) {
						auto layout = ptr->layout();
						release_system(layout);
						auto result = m_systems.emplace(layout, SystemHolder{ std::move(ptr), 0, 0, 0 });
						assert(result.second);
						auto tar = result.first;
						tar->second.ptr->envirment_change(false, true, true);
						for (auto ite = m_systems.begin(); ite != m_systems.end(); ++ite)
						{
							if (ite != tar)
							{
								TickPriority tl1 = ite->second.ptr->tick_layout();
								TickPriority tl2 = tar->second.ptr->tick_layout();
								if (tl1 > tl2)
									m_relationships.push_back(SystemRelationShip{ ite, tar, true, false, {}, {0, 0, 0} });
								else if(tl1 < tl2)
									m_relationships.push_back(SystemRelationShip{ tar, ite, true, false, {}, {0, 0, 0} });
								else {
									auto result = handle_system_conflig(ite, tar);
									if (result)
									{
										auto& [order, force, v_info, index] = *result;
										TickPriority tl1 = ite->second.ptr->tick_priority();
										TickPriority tl2 = tar->second.ptr->tick_priority();
										if (tl1 > tl2)
											m_relationships.push_back(SystemRelationShip{ ite, tar, true, false, {}, {0, 0, 0} });
										else if (tl2 < tl1)
											m_relationships.push_back(SystemRelationShip{ tar, ite, true, false, {}, {0, 0, 0} });
										else {
											TickOrder to1 = ite->second.ptr->tick_order(tar->second.ptr->layout(), v_info.data(), index.data());
											TickOrder to2 = tar->second.ptr->tick_order(ite->second.ptr->layout(), v_info.data(), index.data());
											if (to1 == to2)
											{
												if (to1 == TickOrder::Before || to1 == TickOrder::After)
													order = TickOrder::Undefine;
												else if (to1 == TickOrder::Mutex)
													order = TickOrder::Mutex;
												else if (order == TickOrder::Mutex)
													order = TickOrder::Undefine;
											}
											else if (to1 == TickOrder::After)
												order = TickOrder::After;
											else if (to1 == TickOrder::Before)
												order = TickOrder::Before;
											else if (to2 == TickOrder::After)
												order = TickOrder::Before;
											else if (to2 == TickOrder::Before)
												order = TickOrder::After;
											else
												order = TickOrder::Mutex;
											std::optional<SystemRelationShip> re[2];
											switch (order)
											{
											case TickOrder::Mutex:
												re[0] = SystemRelationShip{ ite, tar, force, true, v_info, index, 0, 0 };
												re[1] = SystemRelationShip{ ite, tar, force, true, v_info, index, 0, 0 };
												break;
											case TickOrder::After:
												re[0] = SystemRelationShip{ ite, tar, force, false, v_info, index, 0, 0 };
												break;
											case TickOrder::Before:
												re[0] = SystemRelationShip{ tar, ite, force, false, v_info, index, 0, 0 };
												break;
											case TickOrder::Undefine:
												release_system(layout);
												{
													Error::SystemOrderConflig tem{ layout.name, ite->second.ptr->layout().name,  index };
													tem.conflig_type.reserve(v_info.size());
													for (auto& ite : v_info)
														tem.conflig_type.push_back(ite.name);
													throw tem;
												}
												break;
											default:
												assert(false);
												break;
											}
											for (size_t i = 0; i < 2; ++i)
											{
												if (re[i].has_value())
												{
													auto ite = std::find_if(m_relationships.rbegin(), m_relationships.rend(), [&](const SystemRelationShip& ship) -> bool {
														return ship.active == (re[i])->active;
													});
													if (ite != m_relationships.rend())
													{
														auto true_ite = ite.base();
														if(true_ite != m_relationships.end())
															++true_ite;
														m_relationships.insert(true_ite, std::move(*re[i]));
													}
													else
														m_relationships.push_back(std::move(*re[i]));
												}
												else
													break;
											}
										}
									}
								}
							}

						}
						m_systems_change = true;
					},
					[&, this](const TypeInfo& layout) {
						m_systems_change = release_system(layout) || m_systems_change;
					}
					}, ite);
			}
			m_regedited_system.clear();
			if (m_systems_change)
			{
				size_t i = 0;
				// 同步 m_relationship m_systems
				for (auto& ite : m_systems)
				{
					ite.second.state_index = i++;
					ite.second.relationship_start_index = 0;
					ite.second.relationship_length = 0;
					ite.second.ptr->envirment_change(true, false, false);
				}
				for(auto& ite : m_using_temporary)
					ite->envirment_change(true, false, false);
				m_conflig_component_state.clear();
				if (!m_relationships.empty())
				{
					auto ite = m_relationships[0].active;
					size_t s = 0;
					size_t k = 0;
					for (; k < m_relationships.size(); ++k)
					{
						handle_relationship_component_conflig(m_relationships[k], m_component_state.size());
						if (m_relationships[k].active != ite)
						{
							ite->second.relationship_start_index = s;
							assert(k > s);
							ite->second.relationship_length = k - s;
							ite = m_relationships[k].active;
							s = k;
						}
					}
					if (k != s)
					{
						ite->second.relationship_start_index = s;
						assert(k > s);
						ite->second.relationship_length = k - s;
					}
				}

				// 找依赖环
				// 0 ： 未访问， 1 ： 访问途中， 2 ：访问完成

				{
					std::vector<size_t> searching_state(m_systems.size(), 0);
					std::vector<std::tuple<SystemHoldMap::iterator, size_t>> searching_stack;
					searching_stack.reserve(m_systems.size());
					while (searching_stack.empty())
					{
						bool all_done = true;
						for (auto ite = m_systems.begin(); ite != m_systems.end(); ++ite)
						{
							if (searching_state[ite->second.state_index] == 0)
							{
								if (ite->second.relationship_length != 0)
								{
									bool hard_relationship = false;
									for (size_t i = 0; i < ite->second.relationship_length; ++i)
									{
										auto& relationship = m_relationships[ite->second.relationship_start_index + i];
										// Mutex 关系的并不是一个依赖
										if (relationship.is_force || !relationship.is_mutex)
										{
											hard_relationship = true;
											break;
										}
									}
									if (hard_relationship)
										continue;
								}
								searching_stack.push_back({ ite , 0 });
								searching_state[ite->second.state_index] = 1;
								break;
							}
						}
						if (!searching_stack.empty())
						{
							while (!searching_stack.empty())
							{
								auto& [ite, r_index] = *searching_stack.rbegin();
								if (r_index == ite->second.relationship_length)
								{
									searching_state[ite->second.state_index] = 2;
									searching_stack.pop_back();
								}
								else {
									auto&  ref = m_relationships[ite->second.relationship_start_index + r_index];
									if (ref.is_force || !ref.is_mutex)
									{
										auto ite2 = ref.passtive;
										size_t state = searching_state[ite2->second.state_index];
										if (state == 1)
										{
											Error::SystemOrderRecursion sor;
											sor.conflig_type_bound.push_back(ref.conflig_bound);
											for (auto& ite : ref.conflig_type)
												sor.conflig_type.push_back(ite.name);
											sor.infos.push_back(ite2->second.ptr->layout().name);
											for (auto ite3 = searching_stack.rbegin(); ite3 != searching_stack.rend(); ++ite3)
											{
												auto [ret, index_r] = *ite3;
												sor.infos.push_back(ret->second.ptr->layout().name);
												auto& ref = m_relationships[ret->second.relationship_start_index + index_r];
												sor.conflig_type_bound.push_back(ref.conflig_bound);
												for (auto& ite : ref.conflig_type)
													sor.conflig_type.push_back(ite.name);
												if (ret == ite2)
													break;
											}
											throw sor;
										}
										else if (state == 0)
										{
											searching_stack.push_back({ ite2 , 0 });
											searching_state[ite2->second.state_index] = 1;
										}
										++r_index;
									}
								}
							}
						}
						else {
							for (auto ite = m_systems.begin(); ite != m_systems.end(); ++ite)
							{
								if (searching_state[ite->second.state_index] == 0)
								{
									searching_stack.push_back({ ite, 0 });
									break;
								}
							}
							if (searching_stack.empty())
								// 全部轮询完了
								break;
						}
					}
				}
				
				// 同步 m_state - m_systems
				m_state.resize(m_systems.size());
				for (auto ite = m_systems.begin(); ite != m_systems.end(); ++ite)
					m_state[ite->second.state_index] = SystemState{ RuningState::Ready, ite->second.relationship_start_index, ite->second.relationship_length, ite };

				// 同步 m_runingrelationship - m_relationship
				m_running_relationship.resize(m_relationships.size());
				for (size_t i = 0; i < m_relationships.size(); ++i)
				{
					auto& tar = m_relationships[i];
					SystemRunningRelationShip running{ tar.passtive->second.state_index, tar.is_force, tar.is_mutex, true, true, true};
					if (!running.is_force)
					{
						bool need_continue = true;
						for (size_t index = 0; index < tar.conflig_bound[0]; ++index)
						{
							auto ti = tar.conflig_type[index];
							auto ite = m_systems.find(ti);
							if (ite != m_systems.end())
							{
								running.system_check = false;
								need_continue = false;
								break;
							}
						}
						if (need_continue)
						{
							for (size_t index = 0; index < tar.conflig_bound[1]; ++index)
							{
								auto ti = tar.conflig_type[tar.conflig_bound[0] + index];
								auto ite = gcp.find(ti);
								if (ite != nullptr)
								{
									running.gobal_component_check = false;
									need_continue = false;
									break;
								}
							}
						}
					}
					m_running_relationship[i] = running;
				}
				m_systems_change = false;
			}
		}
		else if (gobal_component_change || component_change)
		{
			for (auto& ite : m_systems)
				ite.second.ptr->envirment_change(false, gobal_component_change, component_change);
			for (auto& ite : m_using_temporary)
				ite->envirment_change(false, gobal_component_change, component_change);
			if (component_change)
			{
				m_conflig_component_state.clear();
				for (auto& ite : m_relationships)
					handle_relationship_component_conflig(ite, m_component_state.size());
			}

			if (gobal_component_change)
			{
				for (size_t i = 0; i < m_relationships.size(); ++i)
				{
					auto& running = m_running_relationship[i];
					if (!running.is_force && running.system_check)
					{
						auto& rela = m_relationships[i];
						const TypeInfo* ti = rela.conflig_type.data() + rela.conflig_bound[0];
						for (size_t i = 0; i < rela.conflig_bound[1]; ++i)
						{
							auto ti_r = ti[i];
							auto ite = gcp.find(ti_r);
							if (ite != nullptr)
							{
								running.gobal_component_check = false;
								break;
							}
						}
					}
				}
			}
		}
		for (size_t index = 0; index < m_running_relationship.size(); ++index)
		{
			auto& running = m_running_relationship[index];
			if (!running.is_force && (running.system_check || running.gobal_component_check))
			{
				auto& static_rel = m_relationships[index];
				size_t length = static_rel.component_check_length;
				const size_t* state = m_conflig_component_state.data() + static_rel.component_check_start;
				for (size_t i = 0; i < length; ++i)
				{
					if (m_component_state[state[i]])
					{
						running.component_check = false;
						break;
					}
				}
			}
		}
		for (auto& ite : m_state)
			ite.state = RuningState::Ready;
		m_all_done = false;
		m_all_temporary_done = false;
		return !m_systems.empty();
	}

	size_t SystemPool::asynchro_temporary_system(Context* in)
	{
		size_t used = 0;
		while (true)
		{
			SystemInterfacePtr ptr;
			std::lock_guard lg(m_state_mutex);
			for (auto ite = m_using_temporary.begin(); ite != m_using_temporary.end(); ++ite)
			{
				if (*ite)
				{
					ptr = std::move(*ite);
					break;
				}
			}
			if(ptr)
				ptr->apply(in);
			else
			{
				m_using_temporary.clear();
				m_all_temporary_done = true;
				break;
			}
		}
		return used;
	}

	SystemPool::ApplyResult SystemPool::asynchro_apply_system(Context* context, bool wait_for_lock)
	{
		std::optional<SystemHoldMap::iterator> ite;
		{
			if (wait_for_lock)
				m_state_mutex.lock();
			else if (!m_state_mutex.try_lock())
				return ApplyResult::Waitting;
			std::lock_guard lg(m_state_mutex, std::adopt_lock);
			if (!m_all_temporary_done)
				return ApplyResult::Waitting;
			else if (!m_all_done)
			{
				bool all_done = true;
				for (size_t index = 0; index < m_state.size(); ++index)
				{
					auto& state = m_state[index];
					if(state.state != RuningState::Done)
						all_done = false;
					if (state.state == RuningState::Ready)
					{
						size_t start = state.relationship_start_index;
						size_t length = state.relationship_length;
						bool able_to_apply = true;
						for (size_t i = 0; i < length; ++i)
						{
							auto& rel = m_running_relationship[start + i];
							if (rel.is_force || (!rel.system_check || !rel.gobal_component_check || !rel.component_check))
							{
								auto& state2 = m_state[rel.passtive_index];
								if (rel.is_mutex && state2.state == RuningState::Using || !rel.is_mutex && state2.state != RuningState::Done)
								{
									able_to_apply = false;
									break;
								}
							}
						}
						if (able_to_apply)
						{
							state.state = RuningState::Using;
							ite = state.pointer;
							break;
						}
					}
				}
				if (all_done)
				{
					m_all_done = true;
					return ApplyResult::AllDone;
				}
			}
			else
				return ApplyResult::AllDone;
		}
		if (ite.has_value())
		{
			(*ite)->second.ptr->apply(context);
			std::lock_guard lg(m_state_mutex);
			m_state[(*ite)->second.state_index].state = RuningState::Done;
			return ApplyResult::Applied;
		}
		return ApplyResult::Waitting;
	}

	bool SystemPool::release_system(const TypeInfo& id)
	{
		auto ite = m_systems.find(id);
		if (ite != m_systems.end())
		{
			m_relationships.erase(std::remove_if(m_relationships.begin(), m_relationships.end(), [&](const SystemRelationShip& ship) -> bool{
				return ship.active == ite || ship.passtive == ite;
			}), m_relationships.end());
			m_systems.erase(ite);
			return true;
		}
		else
			return false;
	}

	TickOrder handle_system_self_dependence(const TypeInfo& info, TypeInfo const* info_list, ReadWriteProperty const* property, size_t count)
	{
		for (size_t i = 0; i < count; ++i)
		{
			if (info_list[i] == info)
			{
				if (property[i] == ReadWriteProperty::Read)
					return TickOrder::Before;
				else
					return TickOrder::After;
			}
			else if (!(info_list[i] < info))
				break;
		}
		return TickOrder::Undefine;
	}

	std::optional<std::tuple<TickOrder, bool, std::vector<TypeInfo>, std::array<size_t, 3>>> SystemPool::handle_system_conflig(SystemHoldMap::iterator i1, SystemHoldMap::iterator i2)
	{
		assert(i1 != i2);
		TypeInfo const* info1, * info2;
		ReadWriteProperty const* rwp1, * rwp2;
		size_t const* s1, * s2;
		i1->second.ptr->rw_property(info1, rwp1, s1);
		i2->second.ptr->rw_property(info2, rwp2, s2);
		TickOrder to1 = handle_system_self_dependence(i1->first, info2, rwp2, s2[0]);
		TickOrder to2 = handle_system_self_dependence(i2->first, info1, rwp1, s1[0]);
		if (to1 == TickOrder::Undefine && to2 != TickOrder::Undefine)
		{
			if(to2 == TickOrder::Before)
				return std::make_tuple(TickOrder::After, true, std::vector<TypeInfo>{}, std::array<size_t, 3>{0, 0, 0});
			else
				return std::make_tuple(TickOrder::Before, true, std::vector<TypeInfo>{}, std::array<size_t, 3>{0, 0, 0});
		}
		else if (to2 == TickOrder::Undefine && to1 != TickOrder::Undefine)
		{
			return std::make_tuple(to1, true, std::vector<TypeInfo>{}, std::array<size_t, 3>{0, 0, 0});
		}
		else if (to1 == to2 && to2 != TickOrder::Undefine)
		{
			return std::make_tuple(TickOrder::Undefine, true, std::vector<TypeInfo>{}, std::array<size_t, 3>{0, 0, 0});
		}

		std::vector<TypeInfo> conflig_type;
		std::array<size_t, 3> bound;
		TickOrder result = TickOrder::Undefine;
		size_t start1 = 0, start2 = 0;
		for (size_t i = 0; i < 3; ++i)
		{
			size_t count = 0;
			ReadWriteProperty ir1 = ReadWriteProperty::Unknow;
			ReadWriteProperty ir2 = ReadWriteProperty::Unknow;
			for (size_t i1 = 0, i2 = 0; i1 < s1[i] && i2 < s2[i];)
			{
				size_t ti1 = start1 + i1;
				size_t ti2 = start2 + i2;
				auto& info11 = info1[ti1];
				auto& info22 = info2[ti2];
				auto rw11 = rwp1[ti1];
				auto rw22 = rwp2[ti2];
				if (info11 == info22)
				{
					if (!(rw11 == rw22 && rw11 == ReadWriteProperty::Read))
					{
						count += 1;
						conflig_type.push_back(info11);
						ir1 = ir1 > rw11 ? ir1 : rw11;
						ir2 = ir2 > rw22 ? ir2 : rw22;
					}
					++i1; ++i2;
				}
				else if (info11 < info22)
					++i1;
				else
					++i2;
			}
			start1 += s1[i];
			start2 += s2[i];
			if (result != TickOrder::Mutex)
			{
				if (ir1 == ir2)
				{
					if(ir1 == ReadWriteProperty::Write)
						result = TickOrder::Mutex;
				}
				else if (ir1 == ReadWriteProperty::Write)
				{
					if (result == TickOrder::Undefine)
						result = TickOrder::Before;
					else if (result == TickOrder::After)
						result = TickOrder::Mutex;
				}
				else if(ir2 == ReadWriteProperty::Write){
					if (result == TickOrder::Undefine)
						result = TickOrder::After;
					else if (result == TickOrder::Before)
						result = TickOrder::Mutex;
				}
			}
			bound[i] = count;
		}
		if (conflig_type.empty())
			return {};
		else
			return std::make_tuple(result, false, std::move(conflig_type), bound);
	}

	void* SystemPool::find_system(const TypeInfo& ti) noexcept
	{
		auto ite = m_systems.find(ti);
		if (ite != m_systems.end())
			return ite->second.ptr->data();
		else
			return nullptr;
	}

	void SystemPool::clean_all()
	{
		std::unique_lock ul1(m_systems_mutex);
		std::lock_guard lg(m_state_mutex);
		std::lock_guard lg2(m_log_mutex);
		m_systems.clear();
		m_relationships.clear();
		m_regedited_system.clear();
	}

}