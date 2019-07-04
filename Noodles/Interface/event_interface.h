#pragma once
#include "aid.h"
#include <assert.h>
namespace Noodles
{
	namespace Implement
	{

		struct EventPoolMemoryDescription;

		struct EventPoolWrapperInterface
		{
			virtual void construct_event(void(*construct)(void*, void*), void* para, void(*deconstruct)(void*)noexcept) = 0;
			virtual EventPoolMemoryDescription* top_block() const noexcept = 0;
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
			virtual EventPoolWrapperInterface* register_event(const TypeInfo& info) noexcept = 0;
			virtual void unregister_event(const TypeInfo& info) noexcept = 0;
		};
	}

	template<typename EventT>
	struct EventIterator
	{
		const EventT& operator*() const noexcept { return *(static_cast<const EventT*>(m_start->event_start) + m_index); }
		EventIterator& operator++() noexcept
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

		bool operator== (const EventIterator& i) const noexcept { return m_start == i.m_start && m_index == i.m_index; }
		bool operator!= (const EventIterator& i) const noexcept { return !(*this == i); }
	private:
		EventIterator(Implement::EventPoolMemoryDescription* desc) : m_start(desc){}
		Implement::EventPoolMemoryDescription* m_start = nullptr;
		size_t m_index = 0;
		template<typename EventT> friend struct EventViewer;
	};

	template<typename EventT> struct EventViewer
	{
		static_assert(Implement::AcceptableTypeDetector<EventT>::is_pure, "EventProvider only accept pure Type!");

		template<typename ...Parameter> void push(Parameter&& ...pa);
		EventIterator<EventT> begin() const noexcept { return EventIterator<EventT> {m_top}; }
		EventIterator<EventT> end() const noexcept { return EventIterator<EventT>{}; }
	private:
		EventViewer(Implement::EventPoolInterface* pool) noexcept : m_pool(pool) {
			m_wrapper = m_pool->register_event(TypeInfo::create<EventT>());
			assert(m_wrapper != nullptr);
		}

		~EventViewer() {
			m_pool->unregister_event(TypeInfo::create<EventT>());
		}

		static void export_rw_info(Implement::ReadWritePropertyMap& tuple) noexcept {}

		void envirment_change(bool system, bool gobalcomponent, bool component) {}
		void export_type_group_used(const TypeInfo* conflig_type, size_t conflig_count, Implement::ReadWriteProperty*) const noexcept {}

		void pre_apply() noexcept { m_top = m_wrapper->top_block(); }
		void pos_apply() noexcept {}

		Implement::EventPoolInterface* m_pool = nullptr;
		Implement::EventPoolWrapperInterface* m_wrapper = nullptr;
		Implement::EventPoolMemoryDescription* m_top = nullptr;

		template<typename Require> friend struct Implement::FilterAndEventAndSystem;
	};

	template<typename EventT> template<typename ...Parameter> void EventViewer<EventT>::push(Parameter&& ...pa)
	{
		assert(m_wrapper != nullptr);
		auto pa_tuple = std::forward_as_tuple(std::forward<Parameter>(pa)...);
		m_wrapper->construct_event(
			[](void* adress, void* para) {
			auto& po = *static_cast<decltype(pa_tuple)*>(para);
			std::apply([&](auto&& ...at) {
				new (adress) EventT{ std::forward<decltype(at) &&>(at)... };
			}, po);
		}, &pa_tuple, [](void* in) noexcept { reinterpret_cast<EventT*>(in)->~EventT(); });
	}
}
