#pragma once
#include "aid.h"
namespace Noodles
{
	namespace Implement
	{
		struct EventPoolMemoryDescription;

		struct EventPoolWriteWrapperInterface
		{
			virtual void construct_event(void(*construct)(void*, void*), void* para, void(*deconstruct)(void*)noexcept) = 0;
		};

		struct EventPoolMemoryDescription
		{
			EventPoolMemoryDescription* front = nullptr;
			EventPoolMemoryDescription* next = nullptr;
			void (**deconstructor_start)(void*) noexcept = nullptr;
			size_t count = 0;
			void* event_start = nullptr;
		};

		struct EventPoolInterface
		{
			virtual EventPoolMemoryDescription* find_reader(const TypeInfo& layout) noexcept = 0;
			virtual EventPoolWriteWrapperInterface* find_writer_interface(const TypeInfo& layout) noexcept = 0;
		};
	}

	template<typename EventT> struct EventProvider
	{
		static_assert(Implement::AcceptableTypeDetector<EventT>::is_pure, "EventProvider only accept pure Type!");

		operator bool() const noexcept { return m_ref != nullptr; }
		template<typename ...Parameter> void push(Parameter&& ...pa);
	private:
		EventProvider(Implement::EventPoolInterface* pool) noexcept : m_pool(pool) {}

		void type_group_change() noexcept {}
		void system_change() noexcept { m_cur = reinterpret_cast<CompT>(m_pool->find(TypeInfo::create<CompT>())); };
		void gobal_component_change() noexcept { m_ref = m_pool->find_writer_interface(TypeInfo::create<CompT>()); }

		void pre_apply() noexcept {}
		void pos_apply() noexcept {}

		static void export_rw_info(Implement::RWPropertyTuple& tuple) noexcept {}
		Implement::EventPoolWriteWrapperInterface* m_ref = nullptr;
		Implement::EventPoolInterface* m_pool = nullptr;
		template<typename ...Require> friend struct Implement::SystemStorage;
		template<typename Require> friend struct Implement::FilterAndEventAndSystem;
	};

	template<typename EventT> template<typename ...Parameter> void EventProvider<EventT>::push(Parameter&& ...pa)
	{
		assert(m_ref != nullptr);
		auto pa_tuple = std::forward_as_tuple(std::forward<Parameter>(pa)...);
		m_ref->construct_event(
			[](void* adress, void* para) {
			auto& po = *static_cast<decltype(pa_tuple)*>(para);
			std::apply([&](auto&& ...at) {
				new (adress) EventT{ std::forward<decltype(at) &&>(at)... };
			}, po);
		}, &pa_tuple, [](void* in) noexcept { reinterpret_cast<EventT*>(in)->~EventT(); });
	}

	template<typename EventT> void EventProvider<EventT>::lock() noexcept
	{
		m_ref = m_pool->write_lock(TypeLayout::create<EventT>());
		assert(m_ref != nullptr);
	}

	template<typename EventT> void EventProvider<EventT>::unlock() noexcept
	{
		m_ref = nullptr;
	}

	template<typename EventT> struct EventViewer
	{
		static_assert(std::is_same_v<EventT, std::remove_cv_t<std::remove_reference_t<EventT>>>, "EventViewer only accept pure Type!");

		struct iterator
		{
			const EventT& operator*() const noexcept { return *(static_cast<const EventT*>(m_start->event_start) + m_index); }
			iterator operator++() noexcept
			{
				assert(m_start != nullptr);
				++m_index;
				if (m_index >= m_start->count)
				{
					m_start = m_start->next;
					m_index = 0;
				}
				return *this;
			}
			bool operator== (const iterator& i) const noexcept { return m_start == i.m_start && m_index == i.m_index; }
			bool operator!= (const iterator& i) const noexcept { return !(*this == i); }
		private:
			Implement::EventPoolMemoryDescription* m_start = nullptr;
			size_t m_index = 0;
			template<typename EventT> friend struct EventViewer;
		};
		iterator begin() noexcept;
		iterator end() noexcept;
	private:
		EventViewer(Implement::EventPoolInterface* pool) noexcept : m_pool(pool) {}
		void lock() noexcept;
		void unlock() noexcept;
		static void export_rw_info(Implement::RWPropertyTuple& tuple) noexcept {}
		Implement::EventPoolInterface* m_pool = nullptr;
		std::array<std::byte, sizeof(std::shared_lock<std::shared_mutex>) * 2> m_mutex;
		Implement::EventPoolMemoryDescription* m_start = nullptr;
		template<typename ...Require> friend struct Implement::SystemStorage;
		template<typename Require> friend struct Implement::FilterAndEventAndSystem;
	};

	template<typename EventT> void EventViewer<EventT>::lock() noexcept
	{
		m_start = m_pool->read_lock(TypeLayout::create<EventT>(), sizeof(std::shared_lock<std::shared_mutex>) * 2, m_mutex.data());
	}

	template<typename EventT> void EventViewer<EventT>::unlock() noexcept
	{
		m_pool->read_unlock(sizeof(std::shared_lock<std::shared_mutex>) * 2, m_mutex.data());
		m_start = nullptr;
	}

	template<typename EventT> auto EventViewer<EventT>::begin() noexcept -> iterator
	{
		iterator result;
		result.m_start = m_start;
		return result;
	}

	template<typename EventT> auto EventViewer<EventT>::end() noexcept -> iterator
	{
		iterator result;
		return result;
	}
}