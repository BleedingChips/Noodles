#pragma once
#include <vector>
#include <string>
#include <functional>
#include <array>
#include <optional>
#include <regex>
#include <variant>
#include <set>
#include <map>
#include <deque>
#include <assert.h>

namespace Potato::Syntax
{
	enum class Associativity
	{
		Left,
		Right,
	};

	template<typename Terminal>
	struct ast_node_terminal
	{
		Terminal symbol;
		uint64_t data_index;
	};

	template<typename NoTerminal, typename Terminal>
	struct ast_node
	{
		using storage = std::variant<ast_node, ast_node_terminal<Terminal>>;
		struct element : storage
		{
			using storage::variant;
			element(const element&) = default;
			element(element&&) = default;
			element& operator= (const element&) = default;
			element& operator=(element&&) = default;
			storage& var() { return static_cast<storage&>(*this); }
			const storage& var() const { return static_cast<const storage&>(*this); }
			bool is_terminal() const noexcept { return std::holds_alternative<ast_node_terminal<Terminal>>(var()); }
			operator ast_node& () { return std::get<ast_node>(var()); }
			operator const ast_node& () const { return std::get<ast_node>(var()); }
			operator ast_node_terminal<Terminal>& () { return std::get<ast_node_terminal<Terminal>>(var()); }
			operator const ast_node_terminal<Terminal>& () const { return std::get<ast_node_terminal<Terminal>>(var()); }
			ast_node& cast() { return std::get<ast_node>(var()); }
			const ast_node& cast() const { return std::get<ast_node>(var()); }
			ast_node_terminal<Terminal>& cast_terminal() { return std::get<ast_node_terminal<Terminal>>(var()); }
			const ast_node_terminal<Terminal>& cast_terminal() const { return std::get<ast_node_terminal<Terminal>>(var()); }
		};
		ast_node(NoTerminal symbol, uint64_t production, std::vector<element> list) : m_symbol(symbol), m_production(production),
			m_node_list(std::move(list))
		{}
		element& operator[](size_t index) { return m_node_list[index]; }
		const element& operator[](size_t index) const { return m_node_list[index]; }
		size_t size() const { return m_node_list.size(); }
		NoTerminal symbol() const { return m_symbol; }
	private:
		NoTerminal m_symbol;
		uint64_t m_production;
		std::vector<element> m_node_list;
	};

	template<typename NoTerminal, typename Terminal> struct CFG_prodution
	{
		NoTerminal m_head_symbol;
		std::initializer_list<std::variant<NoTerminal, Terminal>> m_production;
		CFG_prodution(NoTerminal start_symbol, std::initializer_list<std::variant<NoTerminal, Terminal>> production)
			: m_head_symbol(start_symbol), m_production(production) {}
		CFG_prodution(CFG_prodution&&) = default;
	};

	template<typename Terminal> struct operator_priority
	{
		std::initializer_list<std::variant<Terminal, std::pair<Terminal, Terminal>>> m_operator;
		Associativity m_associativity;
		operator_priority(operator_priority&&) = default;
		operator_priority(std::initializer_list<std::variant<Terminal, std::pair<Terminal, Terminal>>> ope) : m_operator(ope), m_associativity(Associativity::Left) {}
		operator_priority(std::initializer_list<std::variant<Terminal, std::pair<Terminal, Terminal>>> ope, Associativity ass) : m_operator(ope), m_associativity(ass) {}
	};
	
	namespace Implement
	{

		static constexpr uint64_t flag = 0x8000'0000'0000'0000;

		inline bool is_terminal(uint64_t value) { return value < flag; }

		inline uint64_t terminal_eof() { return (flag - 1); }
		inline uint64_t noterminal_start() { return (0xffff'ffff'ffff'ffff); }
		template<typename NoTerminal> NoTerminal cast_noterminal(uint64_t input)
		{
			assert(input >= flag);
			return static_cast<NoTerminal>(input & (flag - 1));
		}
		template<typename Terminal> Terminal cast_terminal(uint64_t input)
		{
			assert(input < flag);
			return static_cast<Terminal>(input);
		}

		template<typename NoTerminal, typename Terminal> uint64_t cast_symbol(std::variant<NoTerminal, Terminal> input)
		{
			uint64_t value = std::visit([](auto input) { return static_cast<uint64_t>(input); }, input);
			assert(value < flag);
			if (std::holds_alternative<NoTerminal>(input))
				return value | flag;
			else
				return value;
		}

		template<typename NoTerminal, typename Terminal> std::variant<NoTerminal, Terminal> cast_symbol(uint64_t input)
		{
			if (is_terminal(input))
				return static_cast<Terminal>(input);
			else
				return static_cast<NoTerminal>(input & (flag - 1));
		}

		struct shift_reduce_description
		{
			std::map<uint64_t, uint64_t> m_shift;
			std::map<uint64_t, uint64_t> m_reduce;
		};

		struct lr1_reduce_conflict : std::logic_error
		{
			uint64_t m_conflig_token;
			uint64_t m_old_state_index;
			uint64_t m_new_state_index;
			std::vector<std::tuple<uint64_t, std::vector<uint64_t>, std::set<uint64_t>>> m_state;
			lr1_reduce_conflict(uint64_t token, uint64_t old_state_index, uint64_t new_state_index, std::vector<std::tuple<uint64_t, std::vector<uint64_t>, std::set<uint64_t>>>);
		};

		struct lr1_production_head_missing : std::logic_error
		{
			uint64_t m_require_head;
			uint64_t m_production_index;
			lr1_production_head_missing(uint64_t head, uint64_t index);
		};

		struct lr1_same_production : std::logic_error
		{
			uint64_t m_old_production_index;
			uint64_t m_new_production_index;
			std::vector<uint64_t> m_production;
			lr1_same_production(uint64_t old_index, uint64_t new_index, std::vector<uint64_t> production);
		};

		struct lr1_operator_level_conflict : std::logic_error
		{
			uint64_t m_token;
			lr1_operator_level_conflict(uint64_t token) : std::logic_error("operator level conflict"), m_token(token) {}
		};

		struct production_index
		{
			uint64_t m_production_index;
			uint64_t m_production_element_index;
			bool operator<(const production_index& pe) const
			{
				return m_production_index < pe.m_production_index || (m_production_index == pe.m_production_index && m_production_element_index < pe.m_production_element_index);
			}
			bool operator==(const production_index& pe) const
			{
				return m_production_index == pe.m_production_index && m_production_element_index == pe.m_production_element_index;
			}
		};

		struct LR1_implement
		{
			LR1_implement(uint64_t start_symbol, std::vector<std::vector<uint64_t>> production,
				std::vector<std::tuple<std::vector<std::variant<uint64_t, std::pair<uint64_t, uint64_t>>>, Associativity>> input);
			LR1_implement(const uint64_t* input, size_t length);
			size_t calculate_data_length() const noexcept;
			void output_data(uint64_t* output) const noexcept;
		private:
			std::vector<std::tuple<uint64_t, uint64_t>> m_production;
			std::vector<shift_reduce_description> m_table;
			friend struct lr1_processor;
		};

		struct accect {};

		struct lr1_process_error_state {
			std::set<uint64_t> m_shift;
			std::map<uint64_t, uint64_t> m_reduce;
		};

		struct lr1_process_unacceptable_error : std::logic_error, lr1_process_error_state {
			uint64_t m_forward_token;
			lr1_process_unacceptable_error(uint64_t forward_token, lr1_process_error_state lpes);
		};

		struct lr1_process_uncomplete_error : std::logic_error, lr1_process_error_state {
			lr1_process_uncomplete_error(lr1_process_error_state lpes);
		};

		struct lr1_processor
		{
			struct result
			{
				uint64_t reduce_symbol;
				uint64_t reduce_production_index;
				uint64_t element_used;
			};

			lr1_processor(const LR1_implement&);
			std::vector<result> receive(uint64_t symbol);
			auto finish_input() { return receive(terminal_eof()); }
		private:
			const LR1_implement& m_syntax;
			//std::deque<uint64_t> m_buffer;
			std::vector<uint64_t> m_state_stack;
			std::vector<uint64_t> m_input_buffer;
		};

	}

	namespace Error
	{
		template<typename Noterminal, typename Terminal> struct prodution_state
		{
			uint64_t m_index;
			std::vector<std::variant<Noterminal, Terminal>> m_production;
			std::set<Terminal> m_forward;
			prodution_state(const std::tuple<uint64_t, std::vector<uint64_t>, std::set<uint64_t>>& state)
			{
				m_index = std::get<0>(state);
				for (auto& ite : std::get<1>(state))
				{
					if (Implement::is_terminal(ite))
						m_production.push_back(Implement::cast_terminal<Terminal>(ite));
					else
						m_production.push_back(Implement::cast_noterminal<Noterminal>(ite));
				}
				for (auto& ite : std::get<2>(state))
					m_forward.insert(Implement::cast_terminal<Terminal>(ite));
			}
		};

		template<typename Noterminal, typename Terminal>
		struct LR1_reduce_conflict_error : std::logic_error
		{
			Terminal m_token;
			uint64_t m_old_state_index;
			uint64_t m_new_state_index;
			std::vector<prodution_state<Noterminal, Terminal>> m_state;
			LR1_reduce_conflict_error(const Implement::lr1_reduce_conflict& lrc)
				: std::logic_error("reduce comflict"), m_token(Implement::cast_terminal<Terminal>(lrc.m_conflig_token)),
				m_old_state_index(lrc.m_old_state_index), m_new_state_index(lrc.m_new_state_index) {
				for (auto& ite : lrc.m_state)
					m_state.push_back(ite);
			}
		};

		template<typename NoTerminal>
		struct LR1_production_head_missing : std::logic_error
		{
			NoTerminal m_token;
			uint64_t m_index;
			LR1_production_head_missing(const Implement::lr1_production_head_missing& lphm)
				: std::logic_error("unable to find proction head"), m_token(Implement::cast_noterminal<NoTerminal>(lphm.m_require_head)),
				m_index(lphm.m_production_index) {}
		};

		template<typename Terminal>
		struct LR1_operator_level_conflict : std::logic_error
		{
			Terminal m_token;
			LR1_operator_level_conflict(Terminal token) : std::logic_error("operator level conflict"), m_token(token) {}
		};
	}

	/*
	example:

	enum class Terminal
	{
		Num,
		Add,
		Minus,
		Mulyiply,
		Divide,
		Question,
		Colon
	};

	enum class NoTerminal
	{
		Expr,
	};

	LR1<NoTerminal, Terminal> lr1(
	// Start Symbol
	NoTerminal::Expr,
	{
		// Production
		{NoTerminal::Expr, {Terminal::Num}},
		{NoTerminal::Expr, {NoTerminal::Expr, Terminal::Add, NoTerminal::Expr}},
		{NoTerminal::Expr, {NoTerminal::Expr, Terminal::Minus, NoTerminal::Expr}},
		{NoTerminal::Expr, {NoTerminal::Expr, Terminal::Divide, NoTerminal::Expr}},
		{NoTerminal::Expr, {NoTerminal::Expr, Terminal::Mulyiply, NoTerminal::Expr}},
		{NoTerminal::Expr, {NoTerminal::Expr, Terminal::Question, NoTerminal::Expr, Terminal::Colon, NoTerminal::Expr}},
	},
	{
		// Operator Priority
		{ {Terminal::Divide, Terminal::Mulyiply}, Associativity::Left},
		{ {Terminal::Add, Terminal::Minus}, Associativity::Left},
		{ {std::pair{Terminal::Question, Terminal::Colon} }, Associativity::Left},
	}
	);
	*/

	// See the example in the source code
	template<typename NoTerminal, typename Terminal>
	struct LR1 : Implement::LR1_implement
	{
		static_assert(std::is_enum_v<NoTerminal> && std::is_enum_v<Terminal>, "LR1 only accept enum");

		using symbol = std::variant<NoTerminal, Terminal>;
		inline static uint64_t cast_symbol(std::variant<NoTerminal, Terminal> input) { return Implement::cast_symbol(input); }

		LR1(const uint64_t* input, size_t length) : Implement::LR1_implement(input, length) {}


		LR1(
			NoTerminal start_symbol,
			std::initializer_list<CFG_prodution<NoTerminal, Terminal>> production,
			std::initializer_list<operator_priority<Terminal>> priority = {}
		) try : Implement::LR1_implement(cast_symbol(start_symbol), std::move(translate(production)), std::move(translate(priority))) {}
		catch (const Implement::lr1_reduce_conflict& lrc)
		{
			Error::LR1_reduce_conflict_error<NoTerminal, Terminal> error(lrc);
			throw error;
		}
		catch (const Implement::lr1_production_head_missing& lphm)
		{
			Error::LR1_production_head_missing<NoTerminal> error(lphm);
			throw error;
		}

	private:

		static std::vector<std::vector<uint64_t>>
			translate(std::initializer_list<CFG_prodution<NoTerminal, Terminal>> production)
		{
			std::vector<std::vector<uint64_t>> production_tem;
			production_tem.reserve(production.size() + 1);
			for (auto& ite : production)
			{
				std::vector<uint64_t> temporary;
				temporary.reserve(ite.m_production.size() + 1);
				temporary.push_back(cast_symbol(ite.m_head_symbol));
				for (auto ite2 : ite.m_production)
					temporary.push_back(cast_symbol(ite2));
				production_tem.push_back(std::move(temporary));
			}
			return std::move(production_tem);
		}
		static auto
			translate(std::initializer_list<operator_priority<Terminal>> production)
		{
			std::vector<std::tuple<std::vector<std::variant<uint64_t, std::pair<uint64_t, uint64_t>>>, Associativity>> result;
			result.reserve(production.size());
			uint64_t index = 0;
			for (const operator_priority<Terminal>& ite : production)
			{
				std::vector<std::variant<uint64_t, std::pair<uint64_t, uint64_t>>> tem;
				tem.reserve(ite.m_operator.size());
				for (auto& ite2 : ite.m_operator)
				{
					if (std::holds_alternative<Terminal>(ite2))
						tem.push_back(cast_symbol(std::get<Terminal>(ite2)));
					else {
						auto& ref = std::get<std::pair<Terminal, Terminal>>(ite2);
						tem.push_back(std::pair{ cast_symbol(ref.first), cast_symbol(ref.second) });
					}
				}
				result.push_back({ std::move(tem), ite.m_associativity });
			}
			return std::move(result);
		}
	};

	namespace Error
	{
		template<typename Terminal>
		struct generate_ast_error_state
		{
			std::set<Terminal> m_shift;
			std::map<Terminal, uint64_t> m_reduce;
		};

		template<typename NoTerminal, typename Terminal>
		struct generate_ast_unacceptable_error : std::logic_error, generate_ast_error_state<Terminal>
		{
			using state_t = generate_ast_error_state<Terminal>;
			generate_ast_unacceptable_error(std::optional<std::tuple<Terminal, uint64_t>> forward_token, state_t state,
				std::vector<typename ast_node<NoTerminal, Terminal>::element> stack)
				: logic_error("unacceptable token"), m_forward_token(forward_token), state_t(std::move(state)),
				m_stack(std::move(stack)) {}
			std::optional<std::tuple<Terminal, uint64_t>> m_forward_token;
			std::vector<typename ast_node<NoTerminal, Terminal>::element> m_stack;
		};

		template<typename Terminal, typename DataType>
		struct generate_ast_uncomplete_error : std::logic_error, generate_ast_error_state<Terminal>
		{
			using state_t = generate_ast_error_state<Terminal>;
			generate_ast_uncomplete_error(state_t state)
				: logic_error("unacceptable eof"), state_t(std::move(state)) {}
		};

		template<typename Terminal>
		struct generate_ast_uncomplete_ast_error : std::logic_error, generate_ast_error_state<Terminal>
		{
			generate_ast_uncomplete_ast_error(generate_ast_error_state<Terminal> state)
				: logic_error("unacceptable eof symbol"), generate_ast_error_state<Terminal>(std::move(state)) {}
		};
	}

	namespace Implement
	{

		template<typename NoTerminal, typename Terminal>
		struct generate_ast_execute
		{
			using ast_node_t = ast_node<NoTerminal, Terminal>;
			using ast_node_terminal_t = ast_node_terminal<Terminal>;


			static Error::generate_ast_error_state<Terminal> cast_error_state(const lr1_process_error_state& lpe)
			{
				std::set<Terminal> shift;
				for (auto& ite : lpe.m_shift)
					if (Implement::is_terminal(ite) && ite != Implement::terminal_eof())
						shift.insert(Implement::cast_terminal<Terminal>(ite));
				std::map<Terminal, uint64_t> reduce;
				for (auto& ite : lpe.m_reduce)
					if (Implement::is_terminal(ite.first) && ite.first != Implement::terminal_eof())
						reduce.insert({ Implement::cast_terminal<Terminal>(ite.first), ite.second });
				return { std::move(shift), std::move(reduce) };
			}

			static void reduce_stack(std::vector<typename ast_node_t::element>& stack, const std::vector<typename Implement::lr1_processor::result>& input)
			{
				for (auto& ite : input)
				{
					if (ite.reduce_symbol != noterminal_start())
					{
						assert(ite.element_used <= stack.size());
						auto ite2 = stack.begin() + (stack.size() - ite.element_used);
						std::vector<typename ast_node_t::element> temporary(std::move_iterator(ite2), std::move_iterator(stack.end()));
						stack.erase(ite2, stack.end());
						stack.push_back(ast_node_t(Implement::cast_noterminal<NoTerminal>(ite.reduce_symbol), ite.reduce_production_index, std::move(temporary)));
					}
				}
			}

			template<typename Ite>
			auto operator()(const LR1<NoTerminal, Terminal>& syntax, Ite begin, Ite end) -> ast_node<NoTerminal, Terminal>
			{
				std::vector<typename ast_node_t::element> data_stack;
				uint64_t index = 0;
				Implement::lr1_processor lp(syntax);
				try {
					for (; begin != end; ++begin)
					{
						Terminal input = *begin;
						auto re = lp.receive(Implement::cast_symbol<NoTerminal, Terminal>(input));
						reduce_stack(data_stack, re);
						data_stack.push_back(ast_node_terminal_t{ input, index++ });
					}
					auto re = lp.finish_input();
					reduce_stack(data_stack, re);
					assert(data_stack.size() == 1);
					return std::move(data_stack[0]);
				}
				catch (const Implement::lr1_process_unacceptable_error& error)
				{
					if (begin != end)
						throw Error::generate_ast_unacceptable_error<NoTerminal, Terminal>{ { {*begin, index}}, cast_error_state(error), std::move(data_stack) };
					else
						throw Error::generate_ast_unacceptable_error<NoTerminal, Terminal>{ {}, cast_error_state(error), std::move(data_stack) };
				}
			}
		};
	}

	template<typename NoTerminal, typename Terminal, typename Ite>
	auto generate_ast(const LR1<NoTerminal, Terminal>& syntax, Ite begin, Ite end)
	{
		static_assert(std::is_same_v<std::remove_const_t<std::remove_reference_t<decltype(*begin)>>, Terminal>, "");
		return Implement::generate_ast_execute<NoTerminal, Terminal>{}(syntax, begin, end);
	}
	
};
