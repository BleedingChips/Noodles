#pragma once
#include <atomic>
namespace Potato::Tool
{
	struct intrustive_ptr_default_wrapper
	{
		template<typename type>
		static void add_ref(type* t) noexcept { t->add_ref(); }
		template<typename type>
		static void sub_ref(type* t) noexcept { t->sub_ref(); }
	};

	template<typename type, typename wrapper = intrustive_ptr_default_wrapper>
	struct intrusive_ptr
	{
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
		bool operator!= (const intrusive_ptr& ip) const noexcept { return m_ptr != ip.m_ptr; }
		bool operator<(const intrusive_ptr& ip) const noexcept { return m_ptr < ip.m_ptr; }
		bool operator<=(const intrusive_ptr& ip) const noexcept { return m_ptr <= ip.m_ptr; }
		operator type*() const noexcept { return m_ptr; }
		type& operator*() const noexcept { return *m_ptr; }
		type* operator->() const noexcept { return m_ptr; }

		void reset() noexcept { if (m_ptr != nullptr) { wrapper::sub_ref(m_ptr); m_ptr = nullptr; } }
		operator bool() const noexcept { return m_ptr != nullptr; }

		template<typename o_type, typename = std::enable_if_t<std::is_base_of_v<type, o_type> || std::is_base_of_v<o_type, type>>>
		intrusive_ptr<o_type, wrapper> cast_static() const noexcept { return intrusive_ptr<o_type, wrapper>{static_cast<o_type*>(m_ptr)}; }
	private:
		template<typename o_type, typename wrapper> friend struct intrusive_ptr;
		friend wrapper;
		type * m_ptr;
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
}