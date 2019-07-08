#include "syntax.h"
#include "character_encoding.h"
#include <assert.h>
#include <string>
#include <iostream>

namespace Potato::Syntax
{
	namespace Implement
	{
		std::set<uint64_t> calculate_nullable_set(const std::vector<std::vector<uint64_t>>& production)
		{
			std::set<uint64_t> result;
			bool set_change = true;
			while (set_change)
			{
				set_change = false;
				for (auto& ite : production)
				{
					assert(ite.size() >= 1);
					if (ite.size() == 1)
					{
						set_change |= result.insert(ite[0]).second;
					}
					else {
						bool nullable_set = true;
						for (size_t index = 1; index < ite.size(); ++index)
						{
							size_t symbol = ite[index];
							if (is_terminal(symbol) || result.find(symbol) == result.end())
							{
								nullable_set = false;
								break;
							}
						}
						if (nullable_set)
							set_change |= result.insert(ite[0]).second;
					}
				}
			}
			return result;
		}

		std::map<uint64_t, std::set<uint64_t>> calculate_noterminal_first_set(
			const std::vector<std::vector<uint64_t>>& production,
			const std::set<uint64_t>& nullable_set
		)
		{
			std::map<uint64_t, std::set<uint64_t>> result;
			bool set_change = true;
			while (set_change)
			{
				set_change = false;
				for (auto& ite : production)
				{
					assert(ite.size() >= 1);
					for (size_t index = 1; index < ite.size(); ++index)
					{
						auto head = ite[0];
						auto target = ite[index];
						if (is_terminal(target))
						{
							set_change |= result[head].insert(target).second;
							break;
						}
						else {
							if (nullable_set.find(target) == nullable_set.end())
							{
								auto& ref = result[head];
								auto find = result.find(target);
								if (find != result.end())
									for (auto& ite3 : find->second)
										set_change |= ref.insert(ite3).second;
								break;
							}
						}
					}
				}
			}
			return result;
		}

		std::pair<std::set<uint64_t>, bool> calculate_production_first_set(
			std::vector<uint64_t>::const_iterator begin, std::vector<uint64_t>::const_iterator end,
			const std::set<uint64_t>& nullable_set,
			const std::map<uint64_t, std::set<uint64_t>>& first_set
		)
		{
			std::set<uint64_t> temporary;
			for (auto ite = begin; ite != end; ++ite)
			{
				if (Implement::is_terminal(*ite))
				{
					temporary.insert(*ite);
					return { std::move(temporary), false };
				}
				else {
					auto find = first_set.find(*ite);
					if (find != first_set.end())
					{
						temporary.insert(find->second.begin(), find->second.end());
						if (nullable_set.find(*ite) == nullable_set.end())
							return { std::move(temporary), false };
					}
					else
						throw Implement::lr1_production_head_missing{ *ite, 0 };
				}
			}
			return { std::move(temporary), true };
		}

		std::set<uint64_t> calculate_production_first_set_forward(
			std::vector<uint64_t>::const_iterator begin, std::vector<uint64_t>::const_iterator end,
			const std::set<uint64_t>& nullable_set,
			const std::map<uint64_t, std::set<uint64_t>>& first_set,
			const std::set<uint64_t>& forward
		)
		{
			auto result = calculate_production_first_set(begin, end, nullable_set, first_set);
			if (result.second)
				result.first.insert(forward.begin(), forward.end());
			return std::move(result.first);
		}

		std::vector<std::set<uint64_t>> calculate_productions_first_set(
			const std::multimap<uint64_t, std::vector<uint64_t>>& production,
			const std::set<uint64_t>& nullable_set,
			const std::map<uint64_t, std::set<uint64_t>>& first_set_noterminal
		)
		{
			std::vector<std::set<uint64_t>> temporary;
			temporary.reserve(production.size());
			for (auto& ite : production)
				temporary.push_back(std::move(calculate_production_first_set(ite.second.begin(), ite.second.end(), nullable_set, first_set_noterminal).first));
			return temporary;
		}

		bool compress_less()
		{
			return false;
		}

		template<typename T, typename K> int compress_less_implement(T&& t, K&& k)
		{
			if (t < k) return 1;
			if (t == k) return 0;
			return -1;
		}

		template<typename T, typename K, typename ...OT> bool compress_less(T&& t, K&& k, OT&&... ot)
		{
			int result = compress_less_implement(t, k);
			if (result == 1)
				return true;
			if (result == 0)
				return compress_less(std::forward<OT>(ot)...);
			return false;
		}

		struct production_element
		{
			production_index m_index;
			std::set<std::set<uint64_t>>::iterator m_forward_set;
			bool operator<(const production_element& pe) const
			{
				return compress_less(m_index, pe.m_index, &(*m_forward_set), &(*pe.m_forward_set));
			}
			bool operator==(const production_element& pe) const
			{
				return m_index == pe.m_index && m_forward_set == pe.m_forward_set;
			}
		};

		using temporary_state_map = std::map<production_index, std::set<std::set<uint64_t>>::iterator>;

		bool insert_temporary_state_map(production_index index, temporary_state_map& handled, std::set<std::set<uint64_t>>::iterator ite, std::set<std::set<uint64_t>>& total_forward_set)
		{
			auto find_res = handled.insert({ index, ite });
			if (find_res.second)
				return true;
			else {
				if (find_res.first->second != ite)
				{
					std::set<uint64_t> new_set = *find_res.first->second;
					bool change = false;
					for (auto& ite : *ite)
						change = new_set.insert(ite).second || change;
					find_res.first->second = total_forward_set.insert(new_set).first;
					return change;
				}
			}
			return false;
		}

		std::set<uint64_t> remove_forward_set(std::set<uint64_t> current, uint64_t production_index, const std::vector<std::set<uint64_t>>& remove_forward)
		{
			for (auto& ite : remove_forward[static_cast<size_t>(production_index)])
			{
				assert(is_terminal(ite));
				current.erase(ite);
			}
			return std::move(current);
		}

		std::set<production_element> search_direct_mapping(
			temporary_state_map handled,
			std::set<std::set<uint64_t>>& total_forward,
			const std::map<uint64_t, std::set<uint64_t>>& production_map,
			const std::vector<std::vector<uint64_t>>& production,
			const std::set<uint64_t>& nullable_set,
			const std::map<uint64_t, std::set<uint64_t>>& symbol_first_set,
			const std::vector<std::set<uint64_t>>& remove_forward
		)
		{
			std::set<production_index> stack;
			for (auto& ite : handled)
			{
				auto re = stack.insert(ite.first);
				assert(re.second);
			}

			while (!stack.empty())
			{
				auto ite = stack.begin();
				production_index current_index = *ite;
				stack.erase(ite);
				auto find_re = handled.find(current_index);
				assert(find_re != handled.end());

				auto pi = find_re->first;
				assert(production.size() > pi.m_production_index);
				auto& prod = production[static_cast<size_t>(pi.m_production_index)];
				uint64_t ei = pi.m_production_element_index + 1;
				assert(prod.size() >= ei);
				if (prod.size() > ei)
				{
					uint64_t target_symbol = prod[static_cast<size_t>(ei)];
					if (!is_terminal(target_symbol))
					{
						auto find_re2 = production_map.find(target_symbol);
						if (find_re2 != production_map.end())
						{
							assert(!find_re2->second.empty());
							auto forward_set = calculate_production_first_set_forward(prod.begin() + ei + 1, prod.end(), nullable_set, symbol_first_set, *(find_re->second));
							if (!forward_set.empty())
							{
								auto forward_set_ite = total_forward.insert(std::move(forward_set)).first;
								for (auto& current_index : find_re2->second)
								{
									production_index new_one{ current_index , 0 };
									auto o_ite = forward_set_ite;
									if (!remove_forward[current_index].empty())
									{
										auto current_set = remove_forward_set(*o_ite, current_index, remove_forward);
										if (current_set.empty())
											continue;
										o_ite = total_forward.insert(std::move(current_set)).first;
									}
									if (insert_temporary_state_map(new_one, handled, o_ite, total_forward))
										stack.insert(new_one);
								}
							}
						}
						else
							throw lr1_production_head_missing{ target_symbol, production.size() };
					}
				}
			}

			std::set<production_element> result;
			for (auto& ite : handled)
			{
				result.insert({ ite.first, ite.second });
			}
			return result;
		}

		std::tuple<std::map<uint64_t, std::set<production_element>>, std::map<uint64_t, uint64_t>> search_shift_and_reduce(
			const std::set<production_element>& input,
			std::set<std::set<uint64_t>>& total_forward,
			const std::map<uint64_t, std::set<uint64_t>>& production_map,
			const std::vector<std::vector<uint64_t>>& production,
			const std::set<uint64_t>& nullable_set,
			const std::map<uint64_t, std::set<uint64_t>>& symbol_first_set,
			const std::vector<std::set<uint64_t>>& remove_forward
		)
		{
			std::map<uint64_t, temporary_state_map> temporary_shift;
			std::map<uint64_t, uint64_t> reduce;
			assert(!input.empty());
			for (auto& ite : input)
			{
				uint64_t pi = ite.m_index.m_production_index;
				assert(production.size() > pi);
				auto& prod = production[ite.m_index.m_production_index];
				uint64_t ei = ite.m_index.m_production_element_index + 1;
				assert(prod.size() >= ei);
				if (ei == prod.size())
				{
					for (auto& ite2 : *ite.m_forward_set)
					{
						auto re = reduce.insert({ ite2, pi });
						if (!re.second)
						{
							std::vector<std::tuple<uint64_t, std::vector<uint64_t>, std::set<uint64_t>>> state;
							uint64_t old_state = 0;
							uint64_t new_state = 0;
							for (auto& ite : input)
							{
								if (re.first->second == ite.m_index.m_production_index)
									old_state = state.size();
								else if (pi == ite.m_index.m_production_index)
									new_state = state.size();
								state.push_back({ ite.m_index.m_production_element_index, production[ite.m_index.m_production_index], *ite.m_forward_set });
							}
							throw lr1_reduce_conflict{ ite2, old_state, new_state, std::move(state) };
						}

					}
				}
				else {
					uint64_t target_symbol = prod[ei];
					auto cur = ite;
					cur.m_index.m_production_element_index += 1;
					auto& ref = temporary_shift[target_symbol];
					insert_temporary_state_map(cur.m_index, ref, cur.m_forward_set, total_forward);
				}
			}
			std::map<uint64_t, std::set<production_element>> shift;
			for (auto& ite : temporary_shift)
			{
				auto re = shift.insert({ ite.first, search_direct_mapping(std::move(ite.second), total_forward, production_map, production, nullable_set, symbol_first_set, remove_forward) });
				assert(re.second);
			}
			return { shift, reduce };
		}

		lr1_reduce_conflict::lr1_reduce_conflict(uint64_t token, uint64_t old_state_index, uint64_t new_state_index, std::vector<std::tuple<uint64_t, std::vector<uint64_t>, std::set<uint64_t>>> state)
			: std::logic_error("reduce conflict"), m_conflig_token(token), m_old_state_index(old_state_index), m_new_state_index(new_state_index), m_state(std::move(state))
		{}

		lr1_production_head_missing::lr1_production_head_missing(uint64_t head, uint64_t production)
			: std::logic_error("unable to find proction head"), m_require_head(head), m_production_index(production)
		{}

		lr1_same_production::lr1_same_production(uint64_t old_index, uint64_t new_index, std::vector<uint64_t> production)
			: std::logic_error("same production"), m_old_production_index(old_index), m_new_production_index(new_index), m_production(std::move(production))
		{}

		std::map<uint64_t, std::set<uint64_t>> translate_operator_priority(
			const std::vector<std::tuple<std::vector<std::variant<uint64_t, std::pair<uint64_t, uint64_t>>>, Associativity>>& priority
		)
		{
			std::map<uint64_t, std::set<uint64_t>> ope_priority;
			uint64_t index = 0;
			for (uint64_t index = 0; index < priority.size(); ++index)
			{
				std::set<uint64_t> current_remove;
				uint64_t target = index;
				if (std::get<1>(priority[index]) == Associativity::Right)
					++target;
				for (; target > 0; --target)
				{
					auto& ref = std::get<0>(priority[target - 1]);
					for (auto& ite : ref)
					{
						if (std::holds_alternative<uint64_t>(ite))
							current_remove.insert(std::get<uint64_t>(ite));
						else
							current_remove.insert(std::get<std::pair<uint64_t, uint64_t>>(ite).first);
					}
				}
				for (auto& ite : std::get<0>(priority[index]))
				{
					uint64_t symbol;
					if (std::holds_alternative<uint64_t>(ite))
						symbol = std::get<uint64_t>(ite);
					else
						symbol = std::get<std::pair<uint64_t, uint64_t>>(ite).second;
					auto re = ope_priority.insert({ symbol, current_remove });
					if (!re.second)
						throw Error::LR1_operator_level_conflict{ symbol };
				}
			}
			return std::move(ope_priority);
		}

		uint64_t diff(const std::vector<uint64_t>& l1, const std::vector<uint64_t>& l2)
		{
			uint64_t index = 0;
			while (l1.size() > index && l2.size() > index)
			{
				if (l1[index] != l2[index])
					break;
				++index;
			}
			return index;
		}

		LR1_implement::LR1_implement(uint64_t start_symbol, std::vector<std::vector<uint64_t>> production,
			std::vector<std::tuple<std::vector<std::variant<uint64_t, std::pair<uint64_t, uint64_t>>>, Associativity>> priority)
			//: m_production(std::move(production))
		{
			production.push_back({ noterminal_start(), start_symbol });

			m_production.reserve(production.size());
			for (auto& ite : production)
				m_production.push_back({ ite[0], ite.size() - 1 });

			auto null_set = calculate_nullable_set(production);
			auto first_set = calculate_noterminal_first_set(production, null_set);

			std::vector<std::set<uint64_t>> remove;
			remove.resize(production.size());

			{

				auto insert_set = [](std::set<uint64_t>& output, const std::map<uint64_t, std::set<uint64_t>>& first_set, uint64_t symbol) {
					if (is_terminal(symbol))
						output.insert(symbol);
					else {
						auto ite = first_set.find(symbol);
						assert(ite != first_set.end());
						output.insert(ite->second.begin(), ite->second.end());
					}
				};


				std::map<uint64_t, std::set<uint64_t>> remove_map = translate_operator_priority(priority);
				for (size_t x = 0; x < production.size(); ++x)
				{
					auto& ref = production[x];
					assert(!ref.empty());

					if (ref.size() >= 2)
					{
						auto symbol = *(ref.rbegin() + 1);
						auto ite = remove_map.find(symbol);
						if (ite != remove_map.end())
							remove[x].insert(ite->second.begin(), ite->second.end());
					}

					for (size_t y = x + 1; y < production.size(); ++y)
					{
						auto& ref2 = production[y];
						assert(!ref.empty());
						if (ref[0] == ref2[0])
						{
							uint64_t index = diff(ref, ref2);
							if (index < ref.size() && index < ref2.size())
								continue;
							else if (index == ref.size() && index == ref2.size())
								throw lr1_same_production{ x, y, production[x] };
							else {
								if (index < ref.size())
								{
									if (ref[index] != ref[0])
										insert_set(remove[x], first_set, ref[index]);
								}
								else {
									if (ref2[index] != ref2[0])
										insert_set(remove[y], first_set, ref2[index]);
								}
							}
						}
					}
				}
			}

			std::map<uint64_t, std::set<uint64_t>> production_map;

			{
				for (size_t index = 0; index < production.size(); ++index)
				{
					auto& ref = production[index];
					assert(ref.size() >= 1);
					production_map[ref[0]].insert(index);
				}
			}

			std::set<std::set<uint64_t>> all_forward_set;


			std::map<std::set<production_element>, uint64_t> state_map_mapping;
			std::vector<decltype(state_map_mapping)::iterator> stack;
			uint64_t current_state = 0;
			{
				auto re = all_forward_set.insert({ terminal_eof() }).first;
				temporary_state_map temmap{ {production_index{production.size() - 1, 0}, re} };
				auto result = search_direct_mapping(std::move(temmap), all_forward_set, production_map, production, null_set, first_set, remove);
				auto result2 = state_map_mapping.insert({ std::move(result), current_state });
				assert(result2.second);
				++current_state;
				stack.push_back(result2.first);
			}
			std::map<uint64_t, shift_reduce_description> temporary_table;

			while (!stack.empty())
			{
				auto ite = *stack.rbegin();
				stack.pop_back();
				auto shift_and_reduce = search_shift_and_reduce(ite->first, all_forward_set, production_map, production, null_set, first_set, remove);
				std::map<uint64_t, uint64_t> shift;
				for (auto& ite2 : std::get<0>(shift_and_reduce))
				{
					auto result = state_map_mapping.insert({ std::move(ite2.second), current_state });
					if (result.second)
					{
						++current_state;
						stack.push_back(result.first);
					}
					auto result2 = shift.insert({ ite2.first, result.first->second });
					assert(result2.second);
				}
				auto re = temporary_table.insert({ ite->second, shift_reduce_description{std::move(shift), std::move(std::get<1>(shift_and_reduce))} });
				assert(re.second);
			}

			m_table.reserve(temporary_table.size());
			for (auto& ite : temporary_table)
			{
				m_table.push_back(std::move(ite.second));
				assert(m_table.size() == ite.first + 1);
			}
		}

		struct input_index_generator
		{
			input_index_generator(const uint64_t* input, size_t length) : m_input(input), m_length(length) { assert(m_input != nullptr); assert(m_length != 0); }
			uint64_t operator()() noexcept
			{
				assert(m_index < m_length);
				return m_input[m_index++];
			}
		private:
			const uint64_t* m_input;
			uint64_t m_length;
			size_t m_index = 0;
		};

		LR1_implement::LR1_implement(const uint64_t* input, size_t length)
		{
			input_index_generator ig(input, length);
			uint64_t production_count = ig();
			m_production.resize(production_count);
			for (auto& ite : m_production)
			{
				uint64_t symbol = ig();
				uint64_t length = ig();
				ite = { symbol, length };
			}
			uint64_t table_count = ig();
			m_table.resize(table_count);
			for (auto& ite : m_table)
			{
				uint64_t shift_count = ig();
				for (uint64_t i = 0; i < shift_count; ++i)
				{
					uint64_t s1 = ig();
					uint64_t s2 = ig();
					auto re = ite.m_shift.insert({ s1,s2 }).second;
					assert(re);
				}
				uint64_t reduce_count = ig();
				for (uint64_t i = 0; i < reduce_count; ++i)
				{
					uint64_t r1 = ig();
					uint64_t r2 = ig();
					auto re = ite.m_reduce.insert({ r1,r2 }).second;
					assert(re);
				}
			}
		}

		size_t LR1_implement::calculate_data_length() const noexcept
		{
			size_t count = 0;
			count += m_production.size() * 2 + 1;
			count += 1;
			for (auto& ite : m_table)
			{
				count += ite.m_shift.size() * 2 + 1;
				count += ite.m_reduce.size() * 2 + 1;
			}
			return count;
		}

		void LR1_implement::output_data(uint64_t* output) const noexcept
		{
			size_t index = 0;
			output[index++] = m_production.size();
			for (auto& ite : m_production)
			{
				output[index++] = std::get<0>(ite);
				output[index++] = std::get<1>(ite);
			}
			output[index++] = m_table.size();
			for (auto& ite : m_table)
			{
				output[index++] = ite.m_shift.size();
				for (auto& ite2 : ite.m_shift)
				{
					output[index++] = ite2.first;
					output[index++] = ite2.second;
				}
				output[index++] = ite.m_reduce.size();
				for (auto& ite2 : ite.m_reduce)
				{
					output[index++] = ite2.first;
					output[index++] = ite2.second;
				}
			}
		}

		lr1_process_unacceptable_error::lr1_process_unacceptable_error(uint64_t forward_token, lr1_process_error_state lpes)
			: std::logic_error("unacceptable token"), m_forward_token(forward_token), lr1_process_error_state(std::move(lpes)) {}

		lr1_process_uncomplete_error::lr1_process_uncomplete_error(lr1_process_error_state lps)
			: std::logic_error("unacceptable eof"), lr1_process_error_state(std::move(lps)) {}

		lr1_processor::lr1_processor(const LR1_implement& syntax) : m_syntax(syntax), m_state_stack({ 0 }) {}

		auto lr1_processor::receive(uint64_t symbol) -> std::vector<result>
		{
			assert(!m_state_stack.empty());
			std::vector<result> re;
			m_input_buffer.push_back(symbol);
			while (!m_input_buffer.empty())
			{
				uint64_t input = *m_input_buffer.rbegin();
				uint64_t state = *m_state_stack.rbegin();
				auto& ref = m_syntax.m_table[state];
				if (auto reduce = ref.m_reduce.find(input); reduce != ref.m_reduce.end())
				{
					uint64_t production_index = reduce->second;
					assert(production_index < m_syntax.m_production.size());
					uint64_t head_symbol;
					uint64_t production_count;
					std::tie(head_symbol, production_count) = m_syntax.m_production[production_index];
					assert(m_state_stack.size() >= production_count);
					m_state_stack.resize(m_state_stack.size() - production_count);

					re.push_back({ head_symbol, production_index, production_count });
					if (head_symbol != noterminal_start())
						m_input_buffer.push_back(head_symbol);
					else
					{
						assert(m_state_stack.size() == 1);
						m_input_buffer.clear();
					}
				}
				else if (auto shift = ref.m_shift.find(input); shift != ref.m_shift.end())
				{
					m_input_buffer.pop_back();
					m_state_stack.push_back(shift->second);
				}
				else {
					std::set<uint64_t> total_shift;
					for (auto& ite : ref.m_shift)
						total_shift.insert(ite.first);
					throw lr1_process_unacceptable_error{ input, {std::move(total_shift), ref.m_reduce} };
				}
			}
			return std::move(re);
		}

	}
}
