#pragma once
#include "tmpcall.h"
#include <assert.h>
#include <atomic>
#include <mutex>
#include <typeindex>
#include <optional>
#include <type_traits>
namespace Potato::Tool
{
	namespace Implement
	{
		template<typename T> struct base_value_inherit {
			T data;
			base_value_inherit() noexcept {}
			base_value_inherit(T t) noexcept : data(std::move(t)) {}
			base_value_inherit(const base_value_inherit&) = default;
			base_value_inherit(base_value_inherit&&) = default;
			base_value_inherit& operator=(const base_value_inherit&) = default;
			base_value_inherit& operator=(base_value_inherit&&) = default;
			operator T& () noexcept { return data; }
			operator T() const noexcept { return data; }
			base_value_inherit& operator=(T t) noexcept { data = t; return *this; }
		};

		template<> struct base_value_inherit<void> {
			base_value_inherit() noexcept {}
			base_value_inherit& operator= (const base_value_inherit&) = default;
			base_value_inherit& operator= (base_value_inherit&&) = default;
			base_value_inherit(const base_value_inherit&) = default;
			base_value_inherit(base_value_inherit&&) = default;
		};
	}

	template<typename T> using inherit_t = std::conditional_t<
		std::is_arithmetic<T>::value || std::is_same_v<T, void>,
		Implement::base_value_inherit<T>, T
	>;

	/* static_mapping */
	namespace Implement
	{
		template<typename equal_ope, typename less_ope, size_t s, size_t o, typename ...input>
		struct static_mapping_implement
		{
			template<typename T, typename equal_func, typename default_func>
			decltype(auto) operator()(T&& t, equal_func&& ef, default_func&& df) const
			{
				using pick_type = TmpCall::call<TmpCall::append<input...>, TmpCall::select_index<std::integral_constant<size_t, (s + o) / 2>>, TmpCall::self>;
				if (equal_ope{}(Tmp::itself<pick_type>(), std::forward<T>(t)))
				{
					return ef(Tmp::itself<pick_type>());
				}
				else if (less_ope{}(Tmp::itself<pick_type>(), std::forward<T>(t)))
				{
					return static_mapping_implement<equal_ope, less_ope, s, (s + o) / 2, input...>{}(std::forward<T>(t), std::forward<equal_func>(ef), std::forward<default_func>(df));
				}
				else
				{
					return static_mapping_implement<equal_ope, less_ope, (s + o) / 2 + 1, o, input...>{}(std::forward<T>(t), std::forward<equal_func>(ef), std::forward<default_func>(df));
				}
			}
		};

		template<class equal_ope, class less_ope, size_t s, typename ...input>
		struct static_mapping_implement<equal_ope, less_ope, s, s, input...>
		{
			template<typename T, typename equal_func, typename default_func>
			decltype(auto) operator()(T&& t, equal_func&& ef, default_func&& df) const
			{
				return df();
			}
		};
	}

	template<typename equal_ope, typename less_ope>
	struct make_static_mapping
	{
		template<typename ...input> struct in
		{
			using type = Implement::static_mapping_implement<equal_ope, less_ope, 0, sizeof...(input), input...>;
		};
	};

	template<class equal_ope, class less_ope, typename ...input>
	using static_mapping_t = Implement::static_mapping_implement<less_ope, equal_ope, 0, sizeof...(input), input...>;

	template<typename T> class aligned_class
	{
		char storage[sizeof(T) + alignof(T) - 1];
		void* get_pointer() const {
			size_t space = sizeof(T) + alignof(T) - 1;
			void* ptr = storage;
			std::align(alignof(T), sizeof(T), ptr, space);
			return ptr;
		}

	public:

		operator T& () { return *static_cast<T*>(get_pointer()); }
		operator const T& () const { return *static_cast<const T*>(get_pointer()); }

		T* operator->() { return static_cast<T*>(get_pointer()); }
		const T* operator->() const { return static_cast<const T*>(get_pointer()); }

		template<typename ...AT> aligned_class(AT&& ...at) {
			new (get_pointer()) T(std::forward<AT>(at)...);
		}

		~aligned_class()
		{
			static_cast<T*>(get_pointer())->~T();
		}
	};

	namespace Implement
	{
		template<size_t s, typename ...T> struct max_align_implement
		{
			static constexpr size_t value = s;
		};

		template<size_t s, typename K, typename ...T> struct max_align_implement<s, K, T...>
		{
			static constexpr size_t value = max_align_implement<(s > alignof(K) ? s : alignof(K)), T...>::value;
		};
	}

	template<typename ...T> struct max_align : public  Implement::max_align_implement<0, T...> {};

	template<typename value_type> struct stack_list
	{
		value_type& type_ref;
		stack_list* front;
		stack_list(value_type& ref, stack_list* f = nullptr) noexcept : type_ref(ref), front(f) {}
	};

	template<typename value_type, typename callable_function, typename ...other_type> decltype(auto) make_stack_list(callable_function&& ca, stack_list<value_type>* sl = nullptr) noexcept
	{
		return ca(sl);
	}

	template<typename value_type, typename callable_function, typename ...other_type> decltype(auto) make_stack_list(callable_function&& ca, stack_list<value_type>* sl, value_type& ref, other_type&& ...ot) noexcept
	{
		stack_list<value_type> temporary{ ref, sl };
		return make_stack_list<value_type>(std::forward<callable_function>(ca), &temporary, std::forward<other_type>(ot)...);
	}

	template<typename value_type, typename callable_function, typename ...other_type> decltype(auto) make_stack_list(callable_function&& ca, value_type& ref, other_type&& ...ot) noexcept
	{
		stack_list<value_type> temporary{ ref };
		return make_stack_list<value_type>(std::forward<callable_function>(ca), &temporary, std::forward<other_type>(ot)...);
	}

	template<typename interface_t = void> struct info_interface : inherit_t<interface_t>
	{
		bool is(std::type_index info) const noexcept { return info == m_info; }
		template<typename type> bool is() const noexcept { return is(typeid(type)); }
		std::type_index info() const noexcept { return m_info; }
		template<typename ...T>
		info_interface(std::type_index info, T&& ... t) : m_info(info), inherit_t<interface_t>(std::forward<T>(t)...) {};
		template<typename T> T& cast() noexcept { assert(is<T>()); return static_cast<T&>(*this); }
		template<typename T> const T& cast() const noexcept { assert(is<T>()); return static_cast<const T&>(*this); }
	protected:
		info_interface(const info_interface& other) : m_info(m_info), inherit_t<interface_t>(static_cast<const interface_t&>(other)) {}
		info_interface(info_interface&& other) : m_info(m_info), inherit_t<interface_t>(static_cast<interface_t&&>(other)) {}
		info_interface& operator=(const info_interface& other) { inherit_t<interface_t>::operator=(static_cast<const interface_t&>(other)); return *this; }
		info_interface& operator=(info_interface&& other) { inherit_t<interface_t>::operator=(static_cast<interface_t&&>(other)); return *this; }
	private:
		std::type_index m_info;
	};

	template<typename Interface, typename HoldType> struct default_deflection_interface : Interface, inherit_t<HoldType>
	{
		template<typename ...AT>
		default_deflection_interface(AT&& ...at) : Interface(typeid(default_deflection_interface)), inherit_t<HoldType>(std::forward<AT>(at)...) {}
	};

	template<template<typename...> class implement_t = default_deflection_interface, typename interface_t = void> struct deflection_interface : info_interface<interface_t>
	{
		using info_interface<interface_t>::info_interface;
		template<typename type> type& cast() noexcept { assert(this->is<type>()); return static_cast<type&>(static_cast<implement_t<deflection_interface, type>&>(*this)); }
		template<typename type> const type& cast() const noexcept { assert(this->is<type>()); return static_cast<const type&>(static_cast<const implement_t<deflection_interface, type>&>(*this)); }
		template<typename T, typename ...AT> bool cast(T&& t, AT&& ...at) noexcept
		{
			using funtype = Tmp::pick_func<typename Tmp::degenerate_func<Tmp::extract_func_t<T>>::type>;
			static_assert(funtype::size == 1, "only receive one parameter");
			using true_type = std::decay_t<typename funtype::template out<Tmp::itself>::type>;
			//static_assert(std::is_base_of<base_interface, true_type>::value, "need derived form base_interface.");
			if (this->is<true_type>())
				return (t(cast<true_type>(), std::forward<AT>(at)...), true);
			return false;
		}
		template<typename type> using instance_t = implement_t<deflection_interface, type>;
	};

	class atomic_reference_count
	{
		std::atomic_size_t ref = 0;

	public:

		void wait_touch(size_t targe_value) const noexcept;

		bool try_add_ref() noexcept;

		void add_ref() noexcept
		{
			assert(static_cast<std::ptrdiff_t>(ref.load(std::memory_order_relaxed)) >= 0);
			ref.fetch_add(1, std::memory_order_relaxed);
		}

		bool sub_ref() noexcept
		{
			assert(static_cast<std::ptrdiff_t>(ref.load(std::memory_order_relaxed)) >= 0);
			return ref.fetch_sub(1, std::memory_order_relaxed) == 1;
		}

		size_t count() const noexcept { return ref.load(std::memory_order_relaxed); }

		atomic_reference_count() noexcept : ref(0) {}
		atomic_reference_count(const atomic_reference_count&) = delete;
		atomic_reference_count& operator= (const atomic_reference_count&) = delete;
		~atomic_reference_count() { assert(ref.load(std::memory_order_relaxed) == 0); }
	};

	template<typename T> struct replace_void { using type = T; };
	template<> struct replace_void<void> { using type = Tmp::itself<void>; };

	template<typename T, typename mutex_t = std::mutex> class scope_lock
	{
		T data;
		mutable mutex_t mutex;
	public:
		template<typename ...construction_para>  scope_lock(construction_para&& ...cp) : data(std::forward<construction_para>(cp)...) {}
		using type = T;
		using mutex_type = mutex_t;

		using type = T;

		T exchange(T t) noexcept {
			std::lock_guard<mutex_t> lg(mutex);
			T tem(std::move(data));
			data = std::move(t);
			return tem;
		}

		T copy() const noexcept {
			std::lock_guard<mutex_t> lg(mutex);
			return data;
		}

		T move() && noexcept {
			std::lock_guard<mutex_t> lg(mutex);
			T tem(std::move(data));
			return tem;
		}

		scope_lock& equal(T t) noexcept
		{
			std::lock_guard<mutex_t> lg(mutex);
			data = std::move(t);
			return *this;
		}

		template<typename callable_object> decltype(auto) lock(callable_object&& obj) noexcept(noexcept(std::forward<callable_object>(obj)(static_cast<T&>(data))))
		{
			std::lock_guard<mutex_t> lg(mutex);
			return std::forward<callable_object>(obj)(static_cast<T&>(data));
		}
		template<typename callable_object> decltype(auto) lock(callable_object&& obj) const noexcept(noexcept(std::forward<callable_object>(obj)(static_cast<const T&>(data))))
		{
			std::lock_guard<mutex_t> lg(mutex);
			return std::forward<callable_object>(obj)(static_cast<const T&>(data));
		}

		template<typename callable_object>  auto try_lock(callable_object&& obj)
			noexcept(noexcept(std::forward<callable_object>(obj)(static_cast<T&>(data))))
			-> std::conditional_t<
			std::is_void_v<decltype(std::forward<callable_object>(obj)(static_cast<T&>(data)))>,
			bool,
			std::optional<decltype(std::forward<callable_object>(obj)(static_cast<T&>(data)))>
			>
		{
			if (mutex.try_lock())
			{
				std::lock_guard<mutex_t> lg(mutex, std::adopt_lock);
				if constexpr (std::is_void_v<decltype(std::forward<callable_object>(obj)(static_cast<T&>(data)))>)
				{
					return true;
				}
				else
					return{ std::forward<callable_object>(obj)(static_cast<T&>(data)) };
			}
			if constexpr (std::is_void_v<decltype(std::forward<callable_object>(obj)(static_cast<T&>(data)))>)
			{
				return false;
			}
			else
				return {};
		}

		template<typename other_mutex_t, typename other_type, typename callable_object> auto lock_with(scope_lock<other_type, other_mutex_t>& sl, callable_object&& obj)
			noexcept(noexcept(std::forward<callable_object>(obj)(data, sl.data)))
			-> decltype(std::forward<callable_object>(obj)(data, sl.data))
		{
			std::lock(mutex, sl.mutex);
			std::lock_guard<mutex_t> lg(mutex, std::adopt_lock);
			std::lock_guard<other_mutex_t> lg2(sl.mutex, std::adopt_lock);
			return std::forward<callable_object>(obj)(data, sl.data);
		}

	};



	template<size_t ...index, typename tuple_t, typename callable> decltype(auto) apply(std::integer_sequence<size_t, index...>, tuple_t&& t, callable&& c)
	{
		return std::forward<callable>(c)(std::get<index>(std::forward<tuple_t>(t))...);
	}

	inline size_t adjust_alignas_space(size_t require_space, size_t alignas_size)
	{
		size_t sub = require_space % alignas_size;
		return sub == 0 ? require_space : require_space - sub + alignas_size;
	}

	template<size_t aligna> struct alignas(aligna)aligna_buffer {
		static void* allocate(size_t space)
		{
			return new aligna_buffer[adjust_alignas_space(space, aligna)];
		}
		template<typename T> static T* allocate(size_t space)
		{
			return reinterpret_cast<T*>(allocate(space));
		}
		template<typename T> static void release(T * data) noexcept
		{
			delete[] reinterpret_cast<aligna_buffer<aligna>*>(data);
		}
	};

	struct version
	{
		uint64_t m_version = 1;
		version(version&& v) noexcept : m_version(v.m_version) { v.m_version = 1; }
		version(const version&) noexcept : m_version(1) {}
		version()noexcept : m_version(1) {}
		version& operator= (version v) noexcept
		{
			version tem(std::move(v));
			m_version = tem.m_version;
			tem.m_version = 0;
			return *this;
		}
		bool operator ==(const version& v) const noexcept { return m_version == v.m_version; }
		bool update_to(const version& v) noexcept {
			if (m_version != v.m_version)
			{
				m_version = v.m_version;
				return true;
			}
			else
				return false;
		}
		void update() noexcept { m_version++; }
	};

	template<typename T, T default_value> struct integer_moveonly
	{
		T value;
		integer_moveonly() noexcept : value(default_value) {}
		integer_moveonly(T v) noexcept : value(std::move(v)) {}
		integer_moveonly(integer_moveonly&& im) noexcept : value(im.value) { im.value = default_value; }
		operator T () const noexcept { return value; }
		integer_moveonly& operator=(integer_moveonly&& im) noexcept
		{
			integer_moveonly tem(std::move(im));
			value = tem.value;
			tem.value = default_value;
			return *this;
		}
	};

	template<typename T> struct viewer
	{
		//operator T*() const noexcept { return m_ptr; }
		operator const T* () const noexcept { return m_ptr; }
		size_t size() const noexcept { return m_count; }
		viewer(const T* ptr, size_t size) : m_ptr(ptr), m_count(size) {}
		const T& operator [](size_t index) const noexcept { return m_ptr[index]; }
	private:
		const T* m_ptr;
		size_t m_count;
	};

	template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
	template<class... Ts> overloaded(Ts...)->overloaded<Ts...>;

	template<typename Ts, typename = std::void_t<>> struct scope_guard;

	template<typename Ts>
	struct scope_guard<Ts, std::void_t<decltype(std::declval<Ts>()())>> {

		static_assert(std::is_nothrow_invocable_v<Ts>, "scope gurad need noexcept function");
		scope_guard(Ts ts) noexcept : m_function(std::move(ts)), m_dissmise(false) {}
		~scope_guard()  noexcept { if (!m_dissmise) m_function(); }
		void dissmise() noexcept { m_dissmise = true; }
	private:
		Ts m_function;
		bool m_dissmise;
	};

	template<typename Ts> scope_guard(Ts)->scope_guard<Ts>;

	namespace Implement {
		template<size_t s, size_t e> struct sequence_call_implement
		{
			template<typename Tuple, typename Function>
			void operator()(Function&& f, Tuple&& type) {
				std::forward<Function>(f)(std::get<s>(std::forward<Tuple>(type)));
				sequence_call_implement<s + 1, e>{}(std::forward<Function>(f), std::forward<Tuple>(type));
			}
		};
		template<size_t e> struct sequence_call_implement<e, e>
		{
			template<typename Tuple, typename Function>
			void operator()(Function&& f, Tuple&& type) {}
		};
	}

	template<typename Function, typename Tuple> void sequence_call(Function&& f, Tuple&& type)
	{
		Implement::sequence_call_implement<0, std::tuple_size_v<std::remove_reference_t<Tuple>>>{}(std::forward<Function>(f), std::forward<Tuple>(type));
	}
}