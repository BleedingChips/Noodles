#pragma once
#include "tmp.h"
namespace Potato::Tool
{
	// intrustive_ptr ***********************************************************
	struct intrustive_ptr_default_wrapper
	{
		template<typename type>
		static void add_ref(type* t) noexcept { t->add_ref(); }
		template<typename type>
		static void sub_ref(type* t) noexcept { t->sub_ref(); }

		/*
		//overwrite operator();
		template<typename type, typename ...parameters>
		decltype(auto) operator()(const intrusive_ptr<type, intrustive_ptr_default_wrapper>&, parameters&&... a);

		template<typename type, typename ...parameters>
		decltype(auto) operator()(intrusive_ptr<type, intrustive_ptr_default_wrapper>&, parameters&&... a);
		*/
	};

	template<typename type, typename wrapper = intrustive_ptr_default_wrapper>
	struct intrusive_ptr
	{
		static_assert(!std::is_reference_v<type>, "intrusive_ptr : type should not be reference type");

		intrusive_ptr() noexcept : m_ptr(nullptr) {}
		intrusive_ptr(type* t) noexcept : m_ptr(t) { if (t != nullptr) wrapper::add_ref(m_ptr); }
		intrusive_ptr(intrusive_ptr&& ip) noexcept : m_ptr(ip.m_ptr) { ip.m_ptr = nullptr; }
		intrusive_ptr(const intrusive_ptr& ip) noexcept : intrusive_ptr(ip.m_ptr) {}
		~intrusive_ptr() noexcept { reset(); }

		bool operator== (const intrusive_ptr& ip) const noexcept { return m_ptr == ip.m_ptr; }
		bool operator<(const intrusive_ptr& ip) const noexcept { return m_ptr < ip.m_ptr; }

		bool operator!= (const intrusive_ptr& ip) const noexcept { return !((*this) == ip); }
		bool operator<= (const intrusive_ptr& ip) const noexcept { return (*this) == ip || (*this) < ip; }
		bool operator> (const intrusive_ptr& ip) const noexcept { return !((*this) <= ip); }
		bool operator>= (const intrusive_ptr& ip) const noexcept { return !((*this) < ip); }

		operator type* () const noexcept { return m_ptr; }

		template<typename require_type, typename = std::enable_if_t<std::is_convertible_v<std::add_const_t<type*>, require_type*>>>
		operator intrusive_ptr<require_type, wrapper>() const noexcept
		{
			return static_cast<require_type*>(m_ptr);
		}

		template<typename require_type, typename = std::enable_if_t<std::is_convertible_v<type*, require_type*>>>
		operator intrusive_ptr<require_type, wrapper>() noexcept
		{
			return static_cast<require_type*>(m_ptr);
		}

		intrusive_ptr& operator=(intrusive_ptr&& ip) noexcept;
		intrusive_ptr& operator=(const intrusive_ptr& ip) noexcept {
			intrusive_ptr tem(ip);
			return operator=(std::move(tem));
		}

		template<typename ...parameter_types>
		decltype(auto) operator()(parameter_types&& ... i)
		{
			return wrapper{}(*this, std::forward<parameter_types>(i)...);
		}
		
		template<typename ...parameter_types>
		decltype(auto) operator()(parameter_types&& ... i) const
		{
			return wrapper{}(*this, std::forward<parameter_types>(i)...);
		}

		type& operator*() const noexcept { return *m_ptr; }
		type* operator->() const noexcept { return m_ptr; }

		void reset() noexcept { if (m_ptr != nullptr) { wrapper::sub_ref(m_ptr); m_ptr = nullptr; } }
		operator bool() const noexcept { return m_ptr != nullptr; }

		template<typename o_type, typename = std::enable_if_t<std::is_base_of_v<type, o_type> || std::is_base_of_v<o_type, type>>>
		intrusive_ptr<o_type, wrapper> cast_static() const noexcept { return intrusive_ptr<o_type, wrapper>{static_cast<o_type*>(m_ptr)}; }

		template<typename TargetType> TargetType* cast_dynamic() const noexcept {
			if constexpr (std::is_constructible_v<const type*, TargetType*>)
				return *this;
			else
				return dynamic_cast<TargetType*>(m_ptr);
		}

		template<typename TargetType> TargetType* cast_safe() const noexcept { return static_cast<TargetType*>(m_ptr); }
		template<typename TargetType> TargetType* cast_reinterpret() const noexcept { return reinterpret_cast<TargetType*>(m_ptr); }
	private:
		template<typename o_type, typename wrapper> friend struct intrusive_ptr;
		friend wrapper;
		type* m_ptr;
	};

	template<typename type, typename wrapper>
	auto intrusive_ptr<type, wrapper>::operator=(intrusive_ptr&& ip) noexcept ->intrusive_ptr &
	{
		intrusive_ptr tem(std::move(ip));
		reset();
		m_ptr = tem.m_ptr;
		tem.m_ptr = nullptr;
		return *this;
	}

	// observer_ptr ****************************************************************************

	struct observer_ptr_default_wrapper
	{
		template<typename type>
		static void add_ref(type* t) noexcept { }
		template<typename type>
		static void sub_ref(type* t) noexcept { }
	};

	template<typename Type> using observer_ptr = intrusive_ptr<Type, observer_ptr_default_wrapper>;
}