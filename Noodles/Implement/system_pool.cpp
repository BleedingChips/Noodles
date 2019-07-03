#include "../include/system_pool.h"
namespace Noodles::Implement
{
	void SystemPool::regedit_system(SystemInterface* in) noexcept
	{
		assert(in != nullptr);
		std::lock_guard lg(m_log_mutex);
		m_regedited_system.push_back(SystemInterfacePtr{in});
	}

	void SystemPool::destory_system(const TypeLayout& info) noexcept
	{
		std::lock_guard lg(m_log_mutex);
		m_regedited_system.push_back(info);
	}

	void SystemPool::regedit_template_system(TemplateSystemInterface* in) noexcept
	{
		assert(in != nullptr);
		std::lock_guard lg(m_template_mutex);
		m_template_system.push_back(in);
	}

	bool SystemPool::update()
	{
		std::lock_guard lg(m_log_mutex);
		std::unique_lock up(m_system_mutex);
		if (!m_regedited_system.empty())
		{
			uint64_t current_index = m_systems.size();
			bool change = false;
			for (auto& ite : m_regedited_system)
			{
				std::visit(Tool::overloaded{
					[&, this](SystemInterfacePtr& ptr) {
						auto layout = ptr->layout();
						release_system(layout);
						SystemHolder tem{ std::move(ptr), m_systems.size(),{},{} };
						auto result = m_systems.emplace(layout, std::move(tem));
						assert(result.second);
						for (auto ite = m_systems.begin(); ite != m_systems.end(); ++ite)
							if (ite != result.first)
								update_new_system_order(ite, result.first);
						change = true;
					},
					[&, this](const TypeLayout& layout) {
						change = release_system(layout) || change;
					}
					}, ite);
			}
			m_regedited_system.clear();
			if (change)
			{
				m_start_system.clear();
				std::vector<HoldType::iterator> index_mapping(m_systems.size(), m_systems.end());
				for (auto ite = m_systems.begin(); ite != m_systems.end(); ++ite)
				{
					index_mapping[ite->second.state_index] = ite;
					bool is_clean = true;
					for (auto& ite : ite->second.mutex_and_dependence)
						if (!std::get<0>(ite))
							is_clean = false;
					if (is_clean)
						m_start_system.push_back(ite);
				}
				std::vector<std::pair<size_t, size_t>> search_time(m_systems.size(), { 0, 0 });
				size_t time = 0;
				std::vector<HoldType::iterator> search_stack;
				if (!m_start_system.empty())
					search_stack = m_start_system;
				else
					search_stack.push_back(m_systems.begin());
				while (!search_stack.empty())
				{
					auto ite = *search_stack.rbegin();
					auto& ref = search_time[ite->second.state_index];
					if (ref.first == 0)
					{
						ref.first = ++time;
						for (auto ite2 = ite->second.derived.begin(); ite2 != ite->second.derived.end(); ++ite2)
						{
							auto& ref = search_time[(*ite2)->second.state_index];
							if (ref.first == 0 && ref.second == 0)
								search_stack.push_back(*ite2);
							else if (ref.first != 0 && ref.second != 0)
								continue;
							else {
								Error::SystemOrderRecursion error;
								auto& layout = (*ite2)->second.ptr->layout();
								for (auto ite3 = search_stack.begin(); ite3 != search_stack.end(); ++ite3)
								{
									if ((*ite3)->second.ptr->layout() == layout)
									{
										for (; ite3 != search_stack.end(); ++ite3)
										{
											auto& ref = (*ite3)->second;
											if (search_time[ref.state_index].first != 0)
												error.infos.push_back(ref.ptr->layout().name);
										}
										break;
									}
								}
								throw error;
							}
						}
					}
					else {
						ref.second = ++time;
						search_stack.pop_back();
					}
						
				}
			}
		}
		std::lock_guard lg2(m_state_mutex);
		m_waitting_list = m_start_system;
		m_state.resize(m_systems.size());
		for (auto& ite : m_state)
			ite = State::Ready;
		return !m_systems.empty() || !m_template_system.empty();
	}

	SystemPool::ApplyResult SystemPool::asynchro_apply_system(Context* context, bool wait_for_lock)
	{
		HoldType::iterator target;
		bool finded = false;
		{
			if (wait_for_lock)
				m_state_mutex.lock();
			else if (!m_state_mutex.try_lock())
				return ApplyResult::Waitting;
			std::lock_guard lg(m_state_mutex, std::adopt_lock);
			if (m_waitting_list.empty())
			{
				for (auto& ite : m_state)
				{
					if (ite != State::Done)
						return ApplyResult::Waitting;
				}
				return ApplyResult::AllDone;
			}
			else {
				std::shared_lock sl(m_system_mutex);
				bool need_update_list = false;
				for (auto& ite : m_waitting_list)
				{
					if (m_state[ite->second.state_index] == State::Ready)
					{
						bool ready = true;
						for (auto& ite2 : ite->second.mutex_and_dependence)
						{
							auto& state = m_state[std::get<1>(ite2)];
							bool mutex = std::get<0>(ite2);
							if (mutex && state == State::Using || (!mutex) && state != State::Done)
							{
								ready = false;
								break;
							}
						}
						if (ready)
						{
							m_state[ite->second.state_index] = State::Using;
							target = ite;
							ite = m_systems.end();
							finded = true;
							need_update_list = true;
							break;
						}
					}
					else {
						ite = m_systems.end();
						need_update_list = true;
					}
				}
				if (need_update_list)
				{
					m_waitting_list.erase(std::remove_if(m_waitting_list.begin(), m_waitting_list.end(), [&](HoldType::iterator ite) {
						return ite == m_systems.end();
						}), m_waitting_list.end());
				}
			}
		}
		if (finded)
		{
			target->second.ptr->apply(context);
			std::lock_guard lg(m_state_mutex);
			m_state[target->second.state_index] = State::Done;
			m_waitting_list.insert(m_waitting_list.end(), target->second.derived.begin(), target->second.derived.end());
			return ApplyResult::Applied;
		}
		return ApplyResult::Waitting;
	}

	void SystemPool::synchro_apply_template_system(Context* context)
	{
		TemplateSystemInterfacePtr ptr;
		{
			std::lock_guard lg(m_template_mutex);
			if (!m_template_system.empty())
			{
				ptr = std::move(*m_template_system.begin());
				m_template_system.pop_front();
			}
		}
		if (ptr)
			ptr->apply(context);
	}

	bool SystemPool::release_system(const TypeLayout& id)
	{
		auto ite = m_systems.find(id);
		if (ite != m_systems.end())
		{
			for (auto ite2 = m_systems.begin(); ite2 != m_systems.end(); ++ite2)
			{
				if (ite != ite2)
				{
					if (ite2->second.state_index > ite->second.state_index)
						--ite2->second.state_index;
					auto& vec = ite2->second.mutex_and_dependence;
					vec.erase(std::remove_if(vec.begin(), vec.end(), [&](std::tuple<bool, size_t> & in) {
						if (std::get<1>(in) == ite->second.state_index)
							return true;
						else if (std::get<1>(in) > ite->second.state_index)
							--std::get<1>(in);
						return false;
						}), vec.end());
					auto& vec2 = ite2->second.derived;
					vec2.erase(std::remove_if(vec2.begin(), vec2.end(), [&](HoldType::iterator in) {
						return in == ite;
						}), vec2.end());
				}
			}
			return true;
		}
		else
			return false;
	}

	TickOrder handle_rw_collide(
		const TypeLayout* slayout, const RWProperty* sstate, size_t sindex,
		const TypeLayout* tlayout, const RWProperty* tstate, size_t tindex
	)
	{
		TickOrder state = TickOrder::Undefine;
		for (size_t si = 0, ti = 0; si < sindex && ti < tindex;)
		{
			auto& sl = slayout[si];
			auto& tl = tlayout[ti];
			if (sl == tl)
			{
				RWProperty st = sstate[si];
				RWProperty tt = tstate[ti];
				if (st == RWProperty::Write && tt == RWProperty::Write)
					return TickOrder::Mutex;
				else if (st == RWProperty::Write && tt == RWProperty::Read)
				{
					if (state == TickOrder::Undefine)
						state = TickOrder::After;
					else if (state == TickOrder::Before)
						return TickOrder::Mutex;
				}
				else if (st == RWProperty::Read && tt == RWProperty::Write)
				{
					if (state == TickOrder::Undefine)
						state = TickOrder::After;
					else if (state == TickOrder::After)
						return TickOrder::Mutex;
				}
				++si;
				++ti;
			}
			else if (sl < tl)
				++si;
			else
				++ti;
		}
		return state;
	}

	void SystemPool::set_system_order(HoldType::iterator source, HoldType::iterator target, TickOrder order)
	{
		switch (order)
		{
		case TickOrder::Mutex:
			source->second.mutex_and_dependence.push_back({ true, target->second.state_index });
			target->second.mutex_and_dependence.push_back({ true, source->second.state_index });
			break;
		case TickOrder::Before:
			target->second.derived.push_back(source);
			source->second.mutex_and_dependence.push_back({ false, target->second.state_index });
			break;
		case TickOrder::After:
			source->second.derived.push_back(target);
			target->second.mutex_and_dependence.push_back({ false, source->second.state_index });
			break;
		default:
			break;
		}
	}

	TickOrder handle_system_rw_info(const TypeLayout* layouts, const RWProperty* states, size_t count, const TypeLayout& type)
	{
		for (size_t i = 0; i < count && (layouts[i] <= type); ++i)
		{
			if (layouts[i] == type)
			{
				if (states[i] == RWProperty::Read)
					return TickOrder::After;
				else
					return TickOrder::Before;
			}
		}
		return TickOrder::Undefine;
	}

	std::optional<TickOrder> handle_system_tick_order(SystemInterfacePtr& S, SystemInterfacePtr& T)
	{
		auto s_pro = S->tick_priority();
		auto t_pro = T->tick_priority();

		if (s_pro == t_pro)
		{
			auto sord = S->tick_order(T->layout());
			auto tord = T->tick_order(S->layout());
			if (sord == tord)
			{
				if (tord == TickOrder::Undefine || tord == TickOrder::Mutex)
					return tord;
				else
					return {};
			}
			else {
				switch (sord)
				{
				case TickOrder::Undefine: return tord;
				case TickOrder::Mutex:
					switch (tord)
					{
					case TickOrder::Undefine: return TickOrder::Mutex;
					default: return tord;
					};
				case TickOrder::Before:
					return TickOrder::After;
				case TickOrder::After:
					return TickOrder::Before;
				default: assert(false); return TickOrder::Undefine;
				}
			}
		}else if (s_pro < t_pro)
			return TickOrder::Before;
		else
			return TickOrder::After;
	}

	void SystemPool::update_new_system_order(HoldType::iterator source, HoldType::iterator target)
	{
		// user define
		auto spriority = source->second.ptr->tick_layout();
		auto tpriority = target->second.ptr->tick_layout();
		if (spriority == tpriority)
		{
			const TypeLayout* slayout;
			const RWProperty* sstate;
			const size_t* sindex;
			const TypeLayout* tlayout;
			const RWProperty* tstate;
			const size_t* tindex;
			source->second.ptr->rw_property(slayout, sstate, sindex);
			TickOrder s_order = handle_system_rw_info(slayout, sstate, sindex[0], target->second.ptr->layout());
			target->second.ptr->rw_property(tlayout, tstate, tindex);
			TickOrder t_order = handle_system_rw_info(tlayout, tstate, tindex[0], source->second.ptr->layout());

			if (s_order != TickOrder::Undefine || t_order != TickOrder::Undefine)
			{
				auto re = handle_system_tick_order(source->second.ptr, target->second.ptr);
				if (!re.has_value())
					throw Error::SystemOrderConflig{ source->second.ptr->layout().name, target->second.ptr->layout().name};
				else if (*re == TickOrder::Undefine)
				{
					if (s_order == TickOrder::Undefine)
						return set_system_order(source, target, t_order);
					else if (t_order == TickOrder::Undefine)
					{
						switch (s_order)
						{
						case TickOrder::After: return set_system_order(source, target, TickOrder::Before);
						case TickOrder::Before: return set_system_order(source, target, TickOrder::After);
						}
					}
					else if (t_order != s_order)
						return set_system_order(source, target, t_order);
					else
						throw Error::SystemOrderConflig{ source->second.ptr->layout().name, target->second.ptr->layout().name };
				}
				else
					return set_system_order(source, target, *re);
			}

			for (size_t i = 0; i < 4; ++i)
			{
				t_order = handle_rw_collide(slayout, sstate, sindex[i], tlayout, tstate, tindex[i]);
				if (t_order != TickOrder::Undefine)
				{
					if (i < 3)
					{
						auto re = handle_system_tick_order(source->second.ptr, target->second.ptr);
						if (!re.has_value())
							throw Error::SystemOrderConflig{ source->second.ptr->layout().name, target->second.ptr->layout().name };
						else if (*re == TickOrder::Undefine)
						{
							switch (t_order)
							{
							case TickOrder::Mutex:
								throw Error::SystemOrderConflig{ source->second.ptr->layout().name, target->second.ptr->layout().name };
							default: return set_system_order(source, target, t_order);
							}
						}
						else
							return set_system_order(source, target, *re);
					}else
						return set_system_order(source, target, t_order);
				}
				slayout += sindex[i];
				sstate += sindex[i];
				tlayout += tindex[i];
				tstate += tindex[i];
			}
		}
		else if (static_cast<size_t>(spriority) < static_cast<size_t>(tpriority))
			return set_system_order(source, target, TickOrder::After);
		else
			return set_system_order(source, target, TickOrder::Before);
	}

	SystemInterface* SystemPool::find_system(const TypeLayout& ti) noexcept
	{
		std::shared_lock sl(m_system_mutex);
		auto ite = m_systems.find(ti);
		if (ite != m_systems.end())
			return ite->second.ptr;
		else
			return nullptr;
	}

	void SystemPool::clean_all()
	{
		std::unique_lock ul1(m_system_mutex);
		std::lock_guard lg(m_state_mutex);
		std::lock_guard lg2(m_log_mutex);
		std::lock_guard lg3(m_template_mutex);
		m_systems.clear();
		m_start_system.clear();
		m_state.clear();
		m_waitting_list.clear();
		m_regedited_system.clear();
		m_template_system.clear();
	}

}