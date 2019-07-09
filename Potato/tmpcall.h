#pragma once
#include "tmp.h"
namespace Potato::TmpCall
{

	namespace Implement
	{
		struct statement_illegal {};

		template<typename S, typename T, typename ...O> struct process_statement
		{
			using type = T;
			using stack = S;
		};

		template<typename S, typename T, typename P, typename ...OT> struct process_statement<S, T, P, OT...>
		{
			template<typename A, typename K, typename H> static auto func_2(
				std::pair<typename H::template in_statement<A, K>::stack, typename H::template in_statement<A, K>::type>*
				//void*
			)
				->std::pair<typename H::template in_statement<A, K>::stack, typename H::template in_statement<A, K>::type>;

			template<typename A, typename K, typename H> static auto func_2(...)->statement_illegal;

			template<typename A, typename K, typename H> static auto func(
				std::pair<A, typename K::template out<H::template in>::type>*
			)
				->std::pair<A, typename K::template out<H::template in>::type>;
			template<typename A, typename K, typename H> static auto func(...) -> decltype(func_2<A, K, H>(nullptr));

			using next_statement = decltype(func<S, T, P>(nullptr));

			static_assert(!std::is_same<next_statement, statement_illegal>::value, "call statement illegal");
			using type = typename process_statement<typename next_statement::first_type, typename next_statement::second_type, OT...>::type;
			using stack = typename process_statement<typename next_statement::first_type, typename next_statement::second_type, OT...>::stack;
		};

		template<typename T> struct call_implemenmt_pick_result
		{
			template<typename K> static auto func(typename K::type*) -> typename K::type;
			template<typename K> static auto func(...)->statement_illegal;
			using type = decltype(func<T>(nullptr));
			static_assert(!std::is_same<type, statement_illegal>::value, "unavailable output of call");
		};


	}

	template<typename ...T> using call = typename Implement::call_implemenmt_pick_result<typename Implement::process_statement<Tmp::static_map<>, T...>::type>::type;

	template<typename ... func> struct def_func
	{
		template<typename s, typename in> struct in_statement
		{
			using type = typename Implement::process_statement<Tmp::static_map<>, in, func...>::type;
			using stack = s;
		};
	};

	template<typename ...T> struct append
	{
		template<template<typename...> class o> struct out
		{
			using type = o<T...>;
		};
		template<typename ...K> struct in
		{
			template<template<typename...> class o> struct out
			{
				using type = o<K..., T...>;
			};
		};
	};

	template<typename ...T> struct sperate_call
	{
		template<typename ...input_t> struct in
		{
			template<template<typename ...> class o> struct out
			{
				using type = o<call<append<input_t>, T...>... >;
			};
		};
	};

	template<typename F> struct first_match_each
	{
		template<typename T, typename ...AT> struct in
		{
			template<template<typename...> class o> struct out
			{
				using type = o<typename F::template in<T, AT>::type... >;
			};
		};
	};

	template<template<typename...> class f, typename ...AT> struct make_func
	{
		template<typename ...i> struct in
		{
			using type = f<i..., AT...>;
			template<template<typename ...> class o> struct out
			{
				using type = o<f<i..., AT...>>;
			};
		};
	};

	template<template<typename...> class f, typename ...AT> struct make_func_t
	{
		template<typename ...i> struct in
		{
			using type = typename f<i..., AT...>::type;
			template<template<typename ...> class o> struct out
			{
				using type = o<typename f<i..., AT...>::type>;
			};
		};
	};

	template<template<typename...> class f, typename ...AT> struct make_func_front
	{
		template<typename ...i> struct in
		{
			using type = f<AT..., i...>;
			template<template<typename ...> class o> struct out
			{
				using type = o<f<AT..., i...>>;
			};
		};
	};

	template<template<typename...> class f, typename ...AT> struct make_func_t_front
	{
		template<typename ...i> struct in
		{
			using type = typename f<AT..., i...>::type;
			template<template<typename ...> class o> struct out
			{
				using type = o<typename f<AT..., i...>::type>;
			};
		};
	};

	template<typename ...T> struct front_append
	{
		template<template<typename...> class o> struct out
		{
			using type = o<T...>;
		};
		template<typename ...K> struct in
		{
			template<template<typename...> class o> struct out
			{
				using type = o<T..., K...>;
			};
		};
	};

	template<typename F> struct packet
	{
		template<typename ...T> struct in
		{
			using type = typename F::template in<T...>;
			template<template<typename...> class o> struct out
			{
				using type = o<typename F::template in<T...>::type>;
			};
		};
	};

	struct unpacket
	{
		template<typename ...T> struct in
		{
			static_assert(sizeof...(T) < 0, "TmpCall unpacket only accept type like tank<T...>");
		};
		template<typename ...T, template<typename ...> class o> struct in<o<T...>>
		{
			template<template<typename ...> class ou> struct out
			{
				using type = ou<T...>;
			};
		};
	};

	struct self
	{
		template<typename ...T> struct in
		{
			static_assert(sizeof...(T) == 1, "self can not handle mulity output");
		};
		template<typename T> struct in<T>
		{
			using type = T;
			template<template<typename...> class o> struct out
			{
				using type = o<T>;
			};
		};
	};

	namespace Implement
	{
		template<template<typename ...> class output, typename result, typename s, typename e> struct make_range_implemenmt;
		template<template<typename ...> class output, typename ...input, typename s, typename e>
		struct make_range_implemenmt<output, std::tuple<input...>, s, e>
		{
			using type = typename make_range_implemenmt<
				output,
				std::tuple<input..., std::integral_constant<decltype(s::value), s::value>>,
				std::integral_constant<decltype(s::value), (s::value + (s::value > e::value ? -1 : 1))>,
				e
			>::type;
		};
		template<template<typename ...> class output, typename ...input, typename e>
		struct make_range_implemenmt<output, std::tuple<input...>, e, e>
		{
			using type = output<input...>;
		};
	}

	template<typename S, typename E> struct make_range
	{
		static_assert(Tmp::bool_and<Tmp::have_value<S>::value, Tmp::have_value<E>::value>::value, "make_range accept type have T::value");
		static_assert(std::is_same<decltype(S::value), decltype(E::value)>::value, "make_range need the same type of value");
		template<template<typename ...> class o> struct out
		{
			using type = typename Implement::make_range_implemenmt<o, std::tuple<>, S, E>::type;
		};
	};
	template<size_t s, size_t e> using make_range_size_t = make_range<std::integral_constant<size_t, s>, std::integral_constant<size_t, e>>;


	namespace Implement
	{
		template<typename result, template<typename...> class out, typename ...input> struct sperate_value_implement;
		template<typename ...result, template<typename...> class out> struct sperate_value_implement<std::tuple<result...>, out>
		{
			using type = out<result...>;
		};
		template<typename ...result, template<typename...> class out, typename its, typename ...other>
		struct sperate_value_implement<std::tuple<result...>, out, its, other...>
		{
			using type = typename sperate_value_implement<std::tuple<result..., its>, out, other...>::type;
		};
		template<typename ...result, template<typename...> class out, typename its_type, its_type ...value, typename ...other>
		struct sperate_value_implement<std::tuple<result...>, out, std::integer_sequence<its_type, value...>, other...>
		{
			using type = typename sperate_value_implement<std::tuple<result..., std::integral_constant<its_type, value>...>, out, other...>::type;
		};
	}

	struct sperate_value
	{
		template<typename ...T> struct in
		{
			template<template<typename ...> class o> struct out
			{
				using type = typename Implement::sperate_value_implement<std::tuple<>, o, T...>::type;
			};
		};
	};

	namespace Implement
	{
		template<typename result, template<typename ...> class o, typename link_result, typename ...input> struct link_value_implement;
		template<typename ...result, template<typename ...> class o, typename link_result>
		struct link_value_implement<std::tuple<result...>, o, link_result>
		{
			using type = o<result..., link_result>;
		};

		template<typename ...result, template<typename ...> class o>
		struct link_value_implement<std::tuple<result...>, o, void>
		{
			using type = o<result...>;
		};

		template<typename ...result, template<typename ...> class o, typename link_result, typename ite_type, ite_type v, typename ...input>
		struct link_value_implement<std::tuple<result...>, o, link_result, std::integral_constant<ite_type, v>, input...>
		{
			using type = typename link_value_implement<
				std::conditional_t<std::is_void<link_result>::value, std::tuple<result...>, std::tuple<result..., link_result> >,
				o,
				std::integer_sequence<ite_type, v>, input...
			>::type;
		};

		template<typename ...result, template<typename ...> class o, typename ite_type, ite_type ...vf, ite_type v, typename ...input>
		struct link_value_implement<std::tuple<result...>, o, std::integer_sequence<ite_type, vf...>, std::integral_constant<ite_type, v>, input...>
		{
			using type = typename link_value_implement< std::tuple<result...>, o, std::integer_sequence<ite_type, vf..., v>, input...>::type;
		};

		template<typename ...result, template<typename ...> class o, typename ite_type, ite_type ...vf, ite_type ...v, typename ...input>
		struct link_value_implement<std::tuple<result...>, o, std::integer_sequence<ite_type, vf...>, std::integer_sequence<ite_type, v...>, input...>
		{
			using type = typename link_value_implement< std::tuple<result...>, o, std::integer_sequence<ite_type, vf..., v...>, input...>::type;
		};

	}

	struct link_value
	{
		template<typename ...i> struct in
		{
			template<template<typename ...> class o> struct out
			{
				using type = typename Implement::link_value_implement<std::tuple<>, o, void, i...>::type;
			};
		};
	};

	template<typename T, typename L> struct label_pair
	{
		using type = T;
		using label = L;
	};

	namespace Implement
	{
		template<bool state, typename F, typename t, typename i> struct label_implemenmt_helper
		{
			using type = typename F::template in<t, i>::type;
		};

		template<typename F, typename t, typename i>
		struct label_implemenmt_helper<false, F, t, i>
		{
			using type = typename F::template in<t>::type;
		};


		template<size_t s, template<typename...> class output, typename func, typename result, typename ...T> struct label_implemenmt;
		template<size_t s, template<typename...> class output, typename func, typename ...result>
		struct label_implemenmt<s, output, func, std::tuple<result...>>
		{
			using type = output<result...>;
		};

		template<size_t s, template<typename...> class output, typename func, typename ...result, typename its, typename ...other>
		struct label_implemenmt<s, output, func, std::tuple<result...>, its, other...>
		{
			using type = typename label_implemenmt<s + 1, output, func, std::tuple
				<
				result...,
				label_pair<its, typename func::template in<its, std::integral_constant<size_t, s>>::type>
				>, other...>::type;
		};
	}

	template<typename F>
	struct label
	{
		template<typename ...i> struct in
		{
			template<template<typename ...> class o> struct out
			{
				using type = typename Implement::label_implemenmt<0, o, F, std::tuple<>, i...>::type;
			};
		};
	};

	template<typename T, typename S> using label_size = std::integral_constant<size_t, sizeof(T)>;
	template<typename T, typename S> using label_serial = S;
	template<typename T> using replace_label = typename T::label;

	namespace Implement
	{

		template<size_t i, typename ...T> struct pick_implement;
		template<size_t i, typename its, typename ...T> struct pick_implement<i, its, T...>
		{
			using type = typename pick_implement<i - 1, T...>::type;
		};
		template<typename its, typename ...T> struct pick_implement<0, its, T...>
		{
			using type = its;
		};

		template<typename F, template<typename...> class o, typename ...input> struct select_index_implement;
		template<size_t i, template<typename...> class o, typename ...input>
		struct select_index_implement<std::integral_constant<size_t, i>, o, input...>
		{
			using type = o<typename pick_implement<i, input...>::type>;
		};

		template<size_t ...i, template<typename...> class o, typename ...input>
		struct select_index_implement<std::integer_sequence<size_t, i...>, o, input...>
		{
			using type = o<typename pick_implement<i, input...>::type...>;
		};
	}

	template<typename F>
	struct select_index
	{
		template<typename ...T> struct in
		{
			template<template<typename ...> class o> struct out
			{
				using type = typename Implement::select_index_implement<F, o, T...>::type;
			};
		};
	};

	namespace Implement
	{
		template<typename F, typename result, template<typename ...> class o, typename ...input> struct select_if_implement;
		template<typename F, typename ...result, template<typename ...> class o, typename its, typename ...input>
		struct select_if_implement<F, std::tuple<result...>, o, its, input...>
		{
			using type = typename select_if_implement
				<
				F,
				std::conditional_t<F::template in<its>::type::value, std::tuple<result..., its>, std::tuple<result...>>,
				o,
				input...
				>::type;
		};

		template<typename F, typename ...result, template<typename ...> class o>
		struct select_if_implement<F, std::tuple<result...>, o>
		{
			using type = o<result...>;
		};
	}

	template<typename F>
	struct select_if
	{
		template<typename ...T> struct in
		{
			template<template<typename ...> class o> struct out
			{
				using type = typename Implement::select_if_implement<F, std::tuple<>, o, T...>::type;
			};
		};
	};

	template<typename index> struct push_stack
	{
		template<typename s, typename state> struct in_statement
		{
			using type = state;
			using stack = Tmp::set_static_map<s, index, state>;
		};
	};

	template<typename index> struct pop_stack
	{
		template<typename s, typename state> struct in_statement
		{
			using type = Tmp::find_static_map<s, index>;
			using stack = s;
		};
	};

	template<typename sta> struct replace_register
	{
		template<typename s, typename state> struct in_statement
		{
			using type = sta;
			using stack = s;
		};
	};

	template<typename F> struct replace
	{
		template<typename ...T> struct in
		{
			template<template<typename ...> class o> struct out
			{
				using type = o<typename F::template in<T>::type...>;
			};
		};
	};

	namespace Implement
	{
		template<typename F, template<typename...> class out, typename its, typename ...T> struct combine_implement
		{
			using type = out<its>;
		};

		template<typename F, template<typename...> class out, typename ite, typename oth, typename ...T>
		struct combine_implement<F, out, ite, oth, T...>
		{
			using type = typename combine_implement<F, out, typename F::template in<ite, oth>::type, T...>::type;
		};
	}

	template<typename F>
	struct combine
	{
		template<typename ...i> struct in
		{
			template<template<typename...> class o> struct out
			{
				using type = typename Implement::combine_implement<F, o, i...>::type;
			};
		};
	};

	namespace Implement
	{
		template<size_t i, typename F, typename result, template<typename ...> class o, typename ...input> struct localizer_implement;
		template<size_t i, typename F, size_t ...result, template<typename ...> class o, typename its, typename ...input>
		struct localizer_implement<i, F, std::integer_sequence<size_t, result...>, o, its, input...>
		{
			using type = typename localizer_implement <
				i + 1,
				F,
				std::conditional_t<F::template in<its>::type::value, std::integer_sequence<size_t, result..., i>, std::integer_sequence<size_t, result...>>,
				o,
				input...
			>::type;
		};
		template<size_t i, typename F, size_t ...result, template<typename ...> class o>
		struct localizer_implement<i, F, std::integer_sequence<size_t, result...>, o>
		{
			using type = o<std::integer_sequence<size_t, result...>>;
		};
	}

	template<typename F>
	struct localizer
	{
		template<typename ...i> struct in
		{
			template<template<typename ...> class o> struct out
			{
				using type = typename Implement::localizer_implement<0, F, std::integer_sequence<size_t>, o, i...>::type;
			};
		};
	};
}