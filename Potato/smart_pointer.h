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
		//overwrite static_cast, call in operator X() or static cast
		template<typename RequireType, typename SourceType>
		static RequireType overwrite_static_cast(SourceType*& source, Tmp::type_placeholder<RequireType>);
		*/
	};

	namespace Implement
	{
		template<typename T> T& ref_val();
		template<typename SourceType, typename RequireType> struct static_cast_overwrite_exist_method
		{
			template<typename WrapperType, typename = decltype(WrapperType::overwrite_static_cast(ref_val<SourceType>(), Tmp::type_placeholder<RequireType>{})) >
			struct method {};
		};
		template<typename source_type, typename require_type, typename wrapper_type> struct support_static_cast_overwrite :
			std::integral_constant<bool, Tmp::member_exist<static_cast_overwrite_exist_method<source_type, require_type>::template method, wrapper_type>::value>
		{};
	}

	template<typename type, typename wrapper = intrustive_ptr_default_wrapper>
	struct intrusive_ptr
	{
		static_assert(!std::is_reference_v<type>, "intrusive_ptr : type should not be reference type");

		intrusive_ptr() noexcept : m_ptr(nullptr) {}

		template<typename in_type, typename = std::enable_if_t<std::is_convertible_v<in_type*, type*>>>
		intrusive_ptr(in_type* t) noexcept : m_ptr(t) { if (t != nullptr) wrapper::add_ref(m_ptr); }

		template<typename in_type, typename = std::enable_if_t<std::is_convertible_v<in_type*, type*>>>
		intrusive_ptr(intrusive_ptr<in_type, wrapper>&& ip) noexcept : m_ptr(ip.m_ptr) { ip.m_ptr = nullptr; }
		template<typename in_type, typename = std::enable_if_t<std::is_convertible_v<in_type*, type*>>>
		intrusive_ptr(const intrusive_ptr<in_type, wrapper>& ip) noexcept : intrusive_ptr(ip.m_ptr) {}

		intrusive_ptr(intrusive_ptr&& ip) noexcept : m_ptr(ip.m_ptr) { ip.m_ptr = nullptr; }
		intrusive_ptr(const intrusive_ptr& ip) noexcept : intrusive_ptr(ip.m_ptr) {}

		~intrusive_ptr() noexcept { reset(); }

		intrusive_ptr& operator=(intrusive_ptr&& ip) noexcept;
		intrusive_ptr& operator=(const intrusive_ptr& ip) noexcept {
			intrusive_ptr tem(ip);
			return operator=(std::move(tem));
		}

		template<typename in_type, typename = std::enable_if_t<std::is_convertible_v<in_type*, type*>>>
		intrusive_ptr& operator=(const intrusive_ptr<in_type, wrapper>& ip) noexcept {
			intrusive_ptr tem(ip);
			return this->operator=(std::move(tem));
		}

		bool operator== (const intrusive_ptr& ip) const noexcept { return m_ptr == ip.m_ptr; }
		bool operator<(const intrusive_ptr& ip) const noexcept { return m_ptr < ip.m_ptr; }

		bool operator!= (const intrusive_ptr& ip) const noexcept { return !((*this) == ip); }
		bool operator<= (const intrusive_ptr& ip) const noexcept { return (*this) == ip || (*this) < ip; }
		bool operator> (const intrusive_ptr& ip) const noexcept { return !((*this) <= ip); }
		bool operator>= (const intrusive_ptr& ip) const noexcept { return !((*this) < ip); }

		template<typename require_type, typename = std::enable_if_t <
			Implement::support_static_cast_overwrite<std::add_const_t<type*>, require_type, wrapper>::value
			|| std::is_convertible_v<const type*, require_type>
			>>
		operator require_type() const noexcept {
			if constexpr (Implement::support_static_cast_overwrite<std::add_const_t<type*>, require_type, wrapper>::value)
				return wrapper::overwrite_static_cast(m_ptr, Tmp::type_placeholder<require_type>{});
			else
				return m_ptr;
		}

		template<typename require_type, typename = std::enable_if_t <
			Implement::support_static_cast_overwrite<type*, require_type, wrapper>::value
			|| std::is_convertible_v<type*, require_type>
			>>
			operator require_type() noexcept {
			if constexpr (Implement::support_static_cast_overwrite<type*, require_type, wrapper>::value)
				return wrapper::overwrite_static_cast(m_ptr, Tmp::type_placeholder<require_type>{});
			else
				return m_ptr;
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