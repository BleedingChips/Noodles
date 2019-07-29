#pragma once
#include <utility>
#include <type_traits>
#include <functional>
#include <tuple>
namespace Potato::Tmp
{
	/* is_one_of */
	template<typename T, typename ...AT> struct is_one_of : std::false_type {};
	template<typename T, typename I, typename ...AT> struct is_one_of<T, I, AT...> : is_one_of<T, AT...> {};
	template<typename T, typename ...AT> struct is_one_of<T, T, AT...> : std::true_type {};
	template<typename T, typename ...AT> constexpr bool is_one_of_v = is_one_of<T, AT...>::value;

	template<typename T, typename ...AT> struct is_not_one_of : std::true_type {};
	template<typename T, typename I, typename ...AT> struct is_not_one_of<T, I, AT...> : is_not_one_of<T, AT...> {};
	template<typename T, typename ...AT> struct is_not_one_of<T, T, AT...> : std::false_type {};



	/* value_add */
	template<typename T, T value, T ...o_value> struct value_add : std::integral_constant<T, value> {};
	template<typename T, T value, T value2, T ...o_value> struct value_add<T, value, value2, o_value...> : value_add<T, value + value2, o_value...> {};


	/* is_repeat */
	template<typename ...AT> struct is_repeat : public std::false_type {};
	template<typename T, typename ...AT>
	struct is_repeat <T, AT...> : std::conditional< is_one_of<T, AT...>::value, std::true_type, is_repeat < AT...>  >::type {};

	template<bool its, bool ...oth> struct bool_and : bool_and<oth...> {};
	template<bool ...oth> struct bool_and<false, oth...> : std::false_type {};
	template<bool its> struct bool_and<its> : std::integral_constant<bool, its> {};

	template<bool its, bool ...oth> struct bool_or : bool_or<oth...> {};
	template<bool ...oth> struct bool_or<true, oth...> : std::true_type {};
	template<bool its> struct bool_or<its> : std::integral_constant<bool, its> {};

	//to do std::in_place
	template<typename T>
	struct itself
	{
		using type = T;
		T operator() ();
		template<typename ...AT> struct in_t
		{
			using type = T;
		};
		itself() {}
		itself(const itself&) {}
	};

	namespace Implement
	{
		template<typename T> struct type_extract_implement;
		template<typename T> struct type_extract_implement<itself<T>> { using type = T; };
	}

	template<typename T> using type_extract_t = typename Implement::type_extract_implement<T>::type;

	template<typename T> using pure_type = std::remove_const_t<std::remove_reference_t<T>>;


	/* instant */
	namespace Implement
	{
		template<template<typename ...> class output, typename ...input> struct instant_implemenmt
		{
			using type = output<input...>;
		};
	}
	template<template<typename ...> class output, typename ...input> struct instant
	{
		template<typename ...other_input> using in = instant<output, input..., other_input...>;
		template<typename ...other_input> using in_t = typename Implement::instant_implemenmt<output, input..., other_input...>::type;
		template<typename ...other_input> using in_t_t = typename Implement::instant_implemenmt<output, input..., other_input...>::type::type;
		template<typename ...other_input> using front_in = instant<output, other_input..., input...>;
		template<typename ...other_input> using front_in_t = typename Implement::instant_implemenmt<output, other_input..., input...>::type;
		template<typename ...other_input> using front_in_t_t = typename Implement::instant_implemenmt<output, other_input..., input...>::type::type;
	};
	template<template<typename ...> class output, typename ...input> using instant_t = typename Implement::instant_implemenmt<output, input... >::type;

	template<typename T, typename K> using bigger_value = std::conditional_t < (T::value < K::value), K, T >;

	template<template<typename ...> class out> struct extract_lable
	{
		template<typename ... i> struct with_implement
		{
			using type = out<typename i::label...>;
		};
		template<typename ...i> using with = typename with_implement<i...>::type;
	};

	template<template<typename ...> class out> struct extract_type
	{
		template<typename ... i> struct with_implement
		{
			using type = out<typename i::original_t...>;
		};
		template<typename ...i> using with = typename with_implement<i...>::type;
	};

	namespace Implement
	{
		template<typename ...AT>
		struct void_implement
		{
			using type = void;
		};
	}

	template<typename ...AT> using void_t = typename Implement::void_implement<AT...>::type;

	namespace Implement
	{
		struct static_map_illegal {};
		template<typename ...T> struct is_all_pair : std::true_type {};
		template<typename T, typename ...OT> struct is_all_pair<T, OT...> : std::false_type {};
		template<typename K, typename T, typename ...OT> struct is_all_pair<std::pair<K, T>, OT...> : is_all_pair<OT...> {};
	}

	template<typename ...T> struct static_map
	{
		static_assert(Implement::is_all_pair<T...>::value, "static_map only receipt std::pair as parameter");
	};
	template<typename T> struct is_static_map :std::false_type {};
	template<typename ...T> struct is_static_map<static_map<T...>> :std::true_type {};

	namespace Implement
	{
		template<typename T, typename index> struct find_static_map_implement;
		template<typename index> struct find_static_map_implement<static_map<>, index> { using type = static_map_illegal; };
		template<typename A, typename B, typename ...T, typename index> struct find_static_map_implement<static_map<std::pair<A, B>, T...>, index> { using type = typename find_static_map_implement<static_map<T...>, index>::type; };
		template<typename B, typename ...T, typename index> struct find_static_map_implement<static_map<std::pair<index, B>, T...>, index> { using type = B; };

		template<typename T, typename upper, typename index, typename value> struct set_static_map_implement;

		template<typename ...upper, typename index, typename value>
		struct set_static_map_implement<static_map<>, static_map<upper...>, index, value> { using type = static_map<upper..., std::pair<index, value>>; };

		template<typename A, typename B, typename ...T, typename ...upper, typename index, typename value>
		struct set_static_map_implement<static_map<std::pair<A, B>, T...>, static_map<upper...>, index, value>
		{
			using type = typename set_static_map_implement<static_map<T...>, static_map<upper..., std::pair<A, B>>, index, value>::type;
		};

		template<typename B, typename ...T, typename ...upper, typename index, typename value>
		struct set_static_map_implement<static_map<std::pair<index, B>, T...>, static_map<upper...>, index, value> { using type = static_map<upper..., std::pair<index, value>, T...>; };
	}

	template<typename T, typename index> using find_static_map = typename Implement::find_static_map_implement<T, index>::type;
	template<typename T, typename index, typename value> using set_static_map = typename Implement::set_static_map_implement<T, static_map<>, index, value>::type;

	namespace Implement
	{
		template<typename T> struct have_value_implement
		{
			template<typename P> static std::true_type func(std::integral_constant<decltype(P::value), P::value>*);
			template<typename P> static std::false_type func(...);
			using type = decltype(func<T>(nullptr));
		};
	}

	template<typename T> struct have_value : Implement::have_value_implement<T>::type {};

	template<typename ...storage_type> struct storage
	{
		template<typename ...inout> using append = storage<storage_type..., inout...>;
		template<template<typename ...> class output_tank> using output = output_tank<storage_type...>;
	};



	namespace Implement
	{
		template<typename hold_type, size_t index> struct index_holder : std::integral_constant<size_t, index>
		{
			using type = hold_type;
		};

		template<size_t start_index, template<typename, size_t> class index_holder, template<typename ...> class output, typename temporary, typename ...all_index>
		struct add_index_implement
		{
			using type = typename temporary::template output<output>;
		};

		template<size_t start_index, template<typename, size_t> class index_holder, template<typename ...> class output, typename temporary, typename this_type, typename ...all_index>
		struct add_index_implement<start_index, index_holder, output, temporary, this_type, all_index...>
			: add_index_implement<start_index + 1, index_holder, output, typename temporary::template append<index_holder<this_type, start_index>>, all_index...> {};



	}

	template<typename input, size_t start_index = 0, template<typename, size_t> class index_holder = Implement::index_holder> struct add_index;
	template<typename ...input, template<typename ...> class hold_tank, size_t start_index, template<typename, size_t> class index_holder>
	struct add_index<hold_tank<input...>, start_index, index_holder>
		: Implement::add_index_implement<start_index, index_holder, hold_tank, storage<>, input...>
	{};

	template<typename input, size_t start_index = 0, template<typename, size_t> class index_holder = Implement::index_holder >
	using add_index_t = typename add_index<input, start_index, index_holder>::type;


	/***** degenerate_func ********/
	template<typename fun_type> struct pick_func;
	template<typename ret, typename ...para> struct pick_func<ret(para...)>
	{
		using return_t = ret;
		template<template<typename...> class o> using out = o<para...>;
		static constexpr size_t size = sizeof...(para);
	};

	template<typename fun_obj> struct degenerate_func;
	template<typename fun_ret, typename ...fun_para> struct degenerate_func<fun_ret(fun_para...)> { using type = fun_ret(fun_para...); };
	template<typename fun_ret, typename ...fun_para> struct degenerate_func<fun_ret(fun_para...)&> { using type = fun_ret(fun_para...); };
	template<typename fun_ret, typename ...fun_para> struct degenerate_func<fun_ret(fun_para...) &&> { using type = fun_ret(fun_para...); };
	template<typename fun_ret, typename ...fun_para> struct degenerate_func<fun_ret(fun_para...) const > { using type = fun_ret(fun_para...); };
	template<typename fun_ret, typename ...fun_para> struct degenerate_func<fun_ret(fun_para...) const & > { using type = fun_ret(fun_para...); };
	template<typename fun_ret, typename ...fun_para> struct degenerate_func<fun_ret(fun_para...) const && > { using type = fun_ret(fun_para...); };
	template<typename fun_ret, typename ...fun_para> struct degenerate_func<fun_ret(fun_para...) volatile> { using type = fun_ret(fun_para...); };
	template<typename fun_ret, typename ...fun_para> struct degenerate_func<fun_ret(fun_para...) const volatile> { using type = fun_ret(fun_para...); };
	template<typename fun> using degenerate_func_t = typename degenerate_func<fun>::type;


	template<typename fun_obj> struct extract_func
	{
		using type = typename extract_func<decltype(&(std::remove_reference_t<std::remove_const_t<fun_obj>>::operator()))>::type;
	};
	template<typename fun_re, typename ...fun_para> struct extract_func<fun_re(fun_para...)>
	{
		using type = fun_re(fun_para...);
	};
	template<typename fun_re, typename ...fun_para> struct extract_func<fun_re(*)(fun_para...)>
	{
		using type = fun_re(fun_para...);
	};
	template<typename fun_re, typename ...fun_para> struct extract_func<fun_re(*&)(fun_para...)>
	{
		using type = fun_re(fun_para...);
	};
	template<typename fun_re, typename ...fun_para> struct extract_func<fun_re(&)(fun_para...)>
	{
		using type = fun_re(fun_para...);
	};
	template<typename owner, typename func_type> struct extract_func<func_type owner::*>
	{
		using type = std::enable_if_t<std::is_function<func_type>::value, func_type>;
	};
	template<typename owner, typename func_type> struct extract_func<func_type owner::*&>
	{
		using type = std::enable_if_t<std::is_function<func_type>::value, func_type>;
	};
	template<typename owner, typename func_type> struct extract_func<func_type owner::*&&>
	{
		using type = std::enable_if_t<std::is_function<func_type>::value, func_type>;
	};
	template<typename owner, typename func_type> struct extract_func<func_type owner::* const&>
	{
		using type = std::enable_if_t<std::is_function<func_type>::value, func_type>;
	};
	template<typename fun> using extract_func_t = typename extract_func<fun>::type;

	template<typename fun> struct is_member_function_pointer : std::false_type {};
	template<typename fun, typename obj> struct is_member_function_pointer<fun obj::*> : std::true_type {};
	template<typename fun, typename obj> struct is_member_function_pointer<fun obj::*&> : std::true_type {};
	template<typename fun, typename obj> struct is_member_function_pointer<fun obj::*&&> : std::true_type {};
	template<typename fun, typename obj> struct is_member_function_pointer<fun obj::* const&> : std::true_type {};

	namespace Implement
	{
		template<typename function, typename ...paramer> class is_callable_execute
		{
			template<typename fun, typename ...pa> static std::true_type fun(decltype(std::invoke(std::declval<fun>(), std::declval<pa>()...))*);
			template<typename fun, typename ...pa> static std::false_type fun(...);
		public:
			using type = decltype(fun<function, paramer...>(nullptr));
		};
	}
	template<typename function, typename ...paramer> using is_callable = typename Implement::is_callable_execute<function, paramer...>::type;

	template<typename T> struct is_integral_constant : std::false_type {};
	template<typename T, T v> struct is_integral_constant<std::integral_constant<T, v>> : std::true_type {};

	template<typename T> struct is_integer_sequence : std::false_type {};
	template<typename T, T ...v> struct is_integer_sequence<std::integer_sequence<T, v...>> : std::true_type {};

	namespace Implement
	{
		template<typename ...t> struct instance_list {};
		template<template<typename ...> class instance_role, typename T, typename = void> struct able_instance_implement {};
		template<template<typename ...> class instance_role, typename ...instance, typename Result> struct able_instance_implement<instance_role, instance_list<instance...>, Result> : std::false_type {};
		template<template<typename ...> class instance_role, typename ...instance> struct able_instance_implement<instance_role, instance_list<instance...>, std::void_t<instance_role<instance...>>> : std::true_type {};
	}

	template<template<typename ...> class instance_role, typename ...instance> using able_instance = Implement::able_instance_implement<instance_role, Implement::instance_list<instance...>>;
	template<template<typename ...> class instance_role, typename ...instance> constexpr bool able_instance_v = Implement::able_instance_implement<instance_role, Implement::instance_list<instance...>>::value;

	template<typename f, typename s> struct link;
	template<typename ...f, template<typename ...> class ft, typename ...s, template<typename ...> class st> struct link<ft<f...>, st<s...>>
	{
		using type = ft<f..., s...>;
	};

	template<typename f, typename s> using link_t = typename link<f, s>::type;

	template<typename f, template<typename ...> class fto> struct replace;

	template<typename ...f, template<typename ...> class ft, template<typename ...> class fto> struct replace<ft<f...>, fto>
	{
		using type = fto<f...>;
	};

	template<typename f, template<typename ...> class fto> using replace_t = typename replace<f, fto>::type;

	namespace Implement
	{
		template<typename output, typename ...type> struct remove_repeat_implement;
		template<typename ...output, template<typename ...> class ot> struct remove_repeat_implement<ot<output...>>
		{
			using type = ot<output...>;
		};

		template<typename ...output, template<typename ...> class ot, typename t, typename ...o_type> struct remove_repeat_implement<ot<output...>, t, o_type...>
		{
			using result_type = std::conditional_t<
				Tmp::is_one_of<t, output...>::value,
				ot<output...>,
				ot<output..., t>
			>;
			using type = typename remove_repeat_implement<result_type, o_type...>::type;
		};

	}

	template<typename f> struct remove_repeat;
	template<typename ...f, template<typename ...> class ft> struct remove_repeat<ft<f...>>
	{
		using type = typename Implement::remove_repeat_implement<ft<>, f...>::type;
	};

	template<typename f> using remove_repeat_t = typename remove_repeat<f>::type;

	template<typename t> using rm_c_t = std::remove_const_t<t>;
	template<typename t> using rm_r_t = std::remove_reference_t<t>;
	template<typename t> using rm_rc_t = rm_c_t<rm_r_t<t>>;
	template<typename t> using rm_cr_t = rm_r_t<rm_c_t<t>>;

	template<typename T, typename K, typename = void> struct comparable_less : std::false_type {};
	template<typename T, typename K> struct comparable_less<T, K, std::void_t<decltype(std::declval<T>() < std::declval<K>())>> : std::true_type {};

	template<typename T, typename K> constexpr bool comparable_less_v = comparable_less<T, K>::value;

	template<typename T, typename K, typename = void> struct comparable_equate : std::false_type {};
	template<typename T, typename K> struct comparable_equate<T, K, std::void_t<decltype(std::declval<T>() == std::declval<K>())> > : std::true_type {};

	template<typename T, typename K> constexpr bool comparable_equate_v = comparable_equate<T, K>::value;
}

namespace Potato::Tmp
{
	namespace Implement
	{
		template<typename Type> struct function_type_extractor_implement;
		template<typename ReturnType, typename ...Parameter> struct function_type_extractor_implement<ReturnType(Parameter...)> {
			using pure_type = ReturnType(Parameter...);
			using return_t = ReturnType;
			template<template<typename ...> class output> using extract_parameter = output<Parameter...>;
			static constexpr bool is_memory_function = false;
			static constexpr bool is_const = false;
			static constexpr bool is_volatile = false;
			static constexpr bool is_noexcept = false;
			static constexpr bool is_move_reference = false;
			static constexpr bool is_reference = false;
			static constexpr bool is_variadic = false;
			static constexpr size_t parameter_count = sizeof...(Parameter);
		};

		// variadic functions such as std::printf
		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...)> : function_type_extractor_implement<ReturnType(Parameter...)> {
			static constexpr bool is_variadic = true;
		};

		// cv qualifiers
		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) const> : function_type_extractor_implement<ReturnType(Parameter...)> {
			static constexpr bool is_const = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) volatile> : function_type_extractor_implement<ReturnType(Parameter...)> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) const volatile> : function_type_extractor_implement<ReturnType(Parameter...) const> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) const> : function_type_extractor_implement<ReturnType(Parameter..., ...)> {
			static constexpr bool is_const = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) volatile> : function_type_extractor_implement<ReturnType(Parameter..., ...)> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) const volatile> : function_type_extractor_implement<ReturnType(Parameter..., ...) const> {
			static constexpr bool is_volatile = true;
		};

		// ref qualifiers

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...)&> : function_type_extractor_implement<ReturnType(Parameter...)> {
			static constexpr bool is_reference = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) const &> : function_type_extractor_implement<ReturnType(Parameter...)&> {
			static constexpr bool is_const = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) volatile &> : function_type_extractor_implement<ReturnType(Parameter...)&> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) const volatile &> : function_type_extractor_implement<ReturnType(Parameter...) const &> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...)&> : function_type_extractor_implement<ReturnType(Parameter...)&> {
			static constexpr bool is_variadic = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) const &> : function_type_extractor_implement<ReturnType(Parameter..., ...)&> {
			static constexpr bool is_const = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) volatile &> : function_type_extractor_implement<ReturnType(Parameter..., ...)&> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) const volatile &> : function_type_extractor_implement<ReturnType(Parameter..., ...) const &> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) &&> : function_type_extractor_implement<ReturnType(Parameter...)> {
			static constexpr bool is_move_reference = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) const &&> : function_type_extractor_implement<ReturnType(Parameter...) &&> {
			static constexpr bool is_const = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) volatile &&> : function_type_extractor_implement<ReturnType(Parameter...) &&> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) const volatile &&> : function_type_extractor_implement<ReturnType(Parameter...) const &&> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) &&> : function_type_extractor_implement<ReturnType(Parameter...) &&> {
			static constexpr bool is_variadic = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) const &&> : function_type_extractor_implement<ReturnType(Parameter..., ...) &&> {
			static constexpr bool is_const = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) volatile &&> : function_type_extractor_implement<ReturnType(Parameter..., ...) &&> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) const volatile &&> : function_type_extractor_implement<ReturnType(Parameter..., ...) const &&> {
			static constexpr bool is_volatile = true;
		};

		// noexcept versions

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) noexcept> : function_type_extractor_implement<ReturnType(Parameter...)> {
			static constexpr bool is_noexcept = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) noexcept> : function_type_extractor_implement<ReturnType(Parameter...) noexcept> {
			static constexpr bool is_variadic = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) const noexcept> : function_type_extractor_implement<ReturnType(Parameter...) noexcept> {
			static constexpr bool is_const = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) volatile noexcept> : function_type_extractor_implement<ReturnType(Parameter...) noexcept> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) const volatile noexcept> : function_type_extractor_implement<ReturnType(Parameter...) const noexcept> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) const noexcept> : function_type_extractor_implement<ReturnType(Parameter..., ...) noexcept> {
			static constexpr bool is_const = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) volatile noexcept> : function_type_extractor_implement<ReturnType(Parameter..., ...) noexcept> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) const volatile noexcept> : function_type_extractor_implement<ReturnType(Parameter..., ...) const noexcept> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) & noexcept> : function_type_extractor_implement<ReturnType(Parameter...)noexcept> {
			static constexpr bool is_reference = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) const & noexcept> : function_type_extractor_implement<ReturnType(Parameter...) & noexcept> {
			static constexpr bool is_const = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) volatile & noexcept> : function_type_extractor_implement<ReturnType(Parameter...) & noexcept> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) const volatile & noexcept> : function_type_extractor_implement<ReturnType(Parameter...) const & noexcept> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) & noexcept> : function_type_extractor_implement<ReturnType(Parameter...) & noexcept> {
			static constexpr bool is_variadic = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) const & noexcept> : function_type_extractor_implement<ReturnType(Parameter..., ...) & noexcept> {
			static constexpr bool is_const = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) volatile & noexcept> : function_type_extractor_implement<ReturnType(Parameter..., ...) & noexcept> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) const volatile & noexcept> : function_type_extractor_implement<ReturnType(Parameter..., ...) const & noexcept> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) && noexcept> : function_type_extractor_implement<ReturnType(Parameter...)noexcept> {
			static constexpr bool is_move_reference = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) const && noexcept> : function_type_extractor_implement<ReturnType(Parameter...) && noexcept> {
			static constexpr bool is_const = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) volatile && noexcept> : function_type_extractor_implement<ReturnType(Parameter...) && noexcept> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter...) const volatile && noexcept> : function_type_extractor_implement<ReturnType(Parameter...) const && noexcept> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) && noexcept> : function_type_extractor_implement<ReturnType(Parameter...) && noexcept> {
			static constexpr bool is_variadic = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) const && noexcept> : function_type_extractor_implement<ReturnType(Parameter..., ...) && noexcept> {
			static constexpr bool is_const = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) volatile && noexcept> : function_type_extractor_implement<ReturnType(Parameter..., ...) && noexcept> {
			static constexpr bool is_volatile = true;
		};

		template<typename ReturnType, typename ...Parameter>
		struct function_type_extractor_implement<ReturnType(Parameter..., ...) const volatile && noexcept> : function_type_extractor_implement<ReturnType(Parameter..., ...) const && noexcept> {
			static constexpr bool is_volatile = true;
		};

		template<typename FuncType, typename Owner>
		struct function_type_extractor_implement<FuncType Owner::*> : function_type_extractor_implement<FuncType> {
			static constexpr bool is_memory_function = true;
			using member_type = Owner;
		};
	}

	template<typename FuncType, typename = std::void_t<>> struct function_type_extractor : Implement::function_type_extractor_implement<FuncType> {};
	template<typename FuncType> struct function_type_extractor<FuncType, std::void_t<decltype(&FuncType::operator())>> : function_type_extractor<decltype(&FuncType::operator())> {};


	// template<typename Type, typename = decltype(Type::a)> struct demo_condition{};
	template<template<typename...> class Condition, typename Target, typename = std::void_t<>> struct member_exist : std::false_type {};
	template<template<typename...> class Condition, typename Target> struct member_exist<Condition, Target, std::void_t<Condition<Target>>> : std::true_type {};

	template<typename Type>
	struct type_placeholder {};

}