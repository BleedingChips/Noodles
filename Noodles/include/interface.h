#pragma once
#include <typeindex>
#include "..//..//Potato/include/intrusive_ptr.h"
#include <array>
#include <map>
#include <shared_mutex>
#include <set>
#include <tuple>
namespace Noodles
{
	using namespace Potato;
	struct TypeLayout
	{
		size_t hash_code;
		size_t size;
		size_t align;
		const char* name;
		~TypeLayout() = default;
		template<typename Type> static const TypeLayout& create() noexcept {
			static TypeLayout type{typeid(Type).hash_code(), sizeof(Type), alignof(Type), typeid(Type).name()};
			return type;
		}
		bool operator<(const TypeLayout& r) const noexcept
		{
			if (hash_code < r.hash_code)
				return true;
			else if (hash_code == r.hash_code)
			{
				if (size < r.size)
					return true;
				else if (size == r.size)
				{
					if (align < r.align)
						return true;
				}
			}
			return false;
		}
		bool operator<=(const TypeLayout& r) const noexcept
		{
			return (*this) == r || (*this) < r;
		}
		bool operator==(const TypeLayout& type) const noexcept
		{
			return hash_code == type.hash_code && size == type.size && align == type.align;
		}
		bool operator!=(const TypeLayout& type) const noexcept
		{
			return !(*this == type);
		}
	};

	struct Context;

	namespace Implement
	{

		enum class RWProperty
		{
			Read,
			Write
		};

		struct RWPropertyTuple
		{
			std::map<TypeLayout, Implement::RWProperty> components;
			std::map<TypeLayout, Implement::RWProperty> gobal_components;
			std::map<TypeLayout, Implement::RWProperty> systems;
			std::set<TypeLayout> events;
		};

		template<typename ...AT> struct ComponentInfoExtractor
		{
			void operator()(std::map<TypeLayout, RWProperty>& result) {}
		};

		template<typename T, typename ...AT> struct ComponentInfoExtractor<T, AT...>
		{
			void operator()(std::map<TypeLayout, RWProperty>& result) {
				if constexpr (std::is_const_v<T>)
					result.insert({ TypeLayout::create<T>(), RWProperty::Read });
				else
					result[TypeLayout::create<T>()] = RWProperty::Write;
				ComponentInfoExtractor<AT...>{}(result);
			}
		};

		template<typename ...Require> struct SystemStorage;
		template<typename Type> struct FilterAndEventAndSystem;
		struct ComponentPoolInterface;
	}

	namespace Implement
	{

		struct TypeGroup;
		struct EntityInterface;

		struct StorageBlockFunctionPair
		{
			void (*destructor)(void*) noexcept = nullptr;
			void (*mover)(void*, void*) noexcept = nullptr;
		};

		struct StorageBlock
		{
			const TypeGroup* m_owner = nullptr;
			StorageBlock* front = nullptr;
			StorageBlock* next = nullptr;
			size_t available_count = 0;
			StorageBlockFunctionPair** functions = nullptr;
			void** datas = nullptr;
			EntityInterface** entitys = nullptr;
		};

		struct EntityInterface
		{
			virtual void add_ref() const noexcept = 0;
			virtual void sub_ref() const noexcept = 0;
			virtual void read(TypeGroup*&, StorageBlock*&, size_t& index) const noexcept = 0;
			virtual void set(TypeGroup*, StorageBlock*, size_t index) noexcept = 0;
			virtual bool have(const TypeLayout*, size_t index) const noexcept = 0;
		};

		using EntityInterfacePtr = Tool::intrusive_ptr<EntityInterface>;
	}

	struct Entity
	{
		operator bool() const noexcept { return m_imp; }
		template<typename ...Type> bool have() const noexcept
		{
			assert(m_imp);
			std::array<TypeLayout, sizeof...(Type)> infos = {TypeLayout::create<Type>()...};
			return m_imp->have(infos.data(), infos.size());
		}
		Entity(const Entity&) = default;
		Entity(Entity&&) = default;
		Entity() = default;
		Entity& operator=(const Entity&) = default;
		Entity& operator=(Entity&&) = default;
		Entity(Implement::EntityInterfacePtr ptr) : m_imp(std::move(ptr)) {}
	private:
		Implement::EntityInterfacePtr m_imp;

		friend struct EntityWrapper;
		template<typename ...CompT> friend struct EntityFilter;
		friend struct Context;
	};

}

namespace Noodles
{
	namespace Implement
	{

		enum class EntityOperator : uint8_t
		{
			Construct = 0,
			Destruct = 1,
			DeleteAll = 2,
			Destory = 3,
		};

		struct ComponentPoolInterface
		{
			template<typename CompT, typename ...Parameter> CompT& construction_component(EntityInterface* owner, Parameter&& ...pa);
			virtual void lock(size_t mutex_size, void* mutex) = 0;
			virtual size_t search_type_group(
				const TypeLayout* require_layout, size_t input_layout_count, size_t* output_layout_index,
				StorageBlock** output_group, size_t buffer_count, size_t& total_count
			) = 0;
			virtual void unlock(size_t mutex_size, void* mutex) noexcept = 0;
			virtual bool loacte_unordered_layouts(const TypeGroup* input, const TypeLayout* require_layout, size_t index, size_t* output) = 0;
			virtual void construct_component(const TypeLayout& layout, void(*constructor)(void*,void*), void* data, EntityInterface*, void(*deconstructor)(void*) noexcept, void(*mover)(void*, void*) noexcept) = 0;
			virtual void deconstruct_component(EntityInterface*, const TypeLayout& layout) noexcept = 0;
			virtual void handle_entity_imp(EntityInterface*, EntityOperator ope) noexcept = 0;
			void entity_destory(EntityInterface* in) { return handle_entity_imp(in, EntityOperator::Destory); }
			void entity_delete_all(EntityInterface* in) { return handle_entity_imp(in, EntityOperator::DeleteAll); }
		};

		template<typename CompT, typename ...Parameter> auto ComponentPoolInterface::construction_component(EntityInterface* owner, Parameter&& ...pa) -> CompT&
		{
			CompT* result = nullptr;
			auto pa_tuple = std::forward_as_tuple(result, std::forward<Parameter>(pa)...);
			construct_component(TypeLayout::create<CompT>(), [](void* adress, void* para) {
				auto& ref = *static_cast<decltype(pa_tuple)*>(para);
				using Type = CompT;
				std::apply([&](auto& ref, auto && ...at) { ref = new (adress) Type{ std::forward<decltype(at)&&>(at)... }; },ref);
			}, & pa_tuple, owner, [](void* in) noexcept { static_cast<CompT*>(in)->~CompT(); }, [](void* target, void* source) noexcept {
				new (target) CompT{std::move(*reinterpret_cast<CompT*>(source))};
			});
			return *result;
		}
	}

	namespace Implement
	{
		template<typename T> struct TypePropertyDetector {
			using type = std::remove_reference_t<std::remove_cv_t<T>>;
			static constexpr bool value = std::is_same_v<T, type> || std::is_same_v<T, std::add_const_t<type>>;
		};

		template<size_t start, size_t end> struct ComponentTupleHelper
		{
			template<typename TupleType>
			static void translate(StorageBlock* block, size_t* index, TupleType& tuple) { 
				std::get<start>(tuple) = 
					reinterpret_cast<std::remove_reference_t<decltype(std::get<start>(tuple))>>(block->datas[index[start]]);
				ComponentTupleHelper<start + 1, end>::translate(block, index, tuple);
			}

			template<typename TupleType>
			static void add(TupleType& tuple) { std::get<start>(tuple) += 1; ComponentTupleHelper<start + 1, end>::add(tuple); }
		};

		template<size_t end> struct ComponentTupleHelper<end, end>
		{
			template<typename TupleType>
			static void translate(StorageBlock* block, size_t* index, TupleType& tuple) {}

			template<typename TupleType>
			static void add(TupleType& tuple) { }
		};

		template<typename ...CompT> struct ComponentTypeInfo
		{
			static const std::array<TypeLayout, sizeof...(CompT)>& info() noexcept { return m_info; }
		private:
			static const std::array<TypeLayout, sizeof...(CompT)> m_info;
		};

		template<typename ...CompT> const std::array<TypeLayout, sizeof...(CompT)> ComponentTypeInfo<CompT...>::m_info = {
			TypeLayout::create<CompT>()...
		};
	}

	template<typename ...CompT> struct EntityFilter
	{
		static_assert(Tmp::bool_and<true, Implement::TypePropertyDetector<CompT>::value...>::value, "EntityFilter only accept Type and const Type!");
		template<typename Func>
		void operator()(const Entity& wrapper, Func&& f);
	private:
		EntityFilter(Implement::ComponentPoolInterface* pool) noexcept : m_pool(pool) { assert(pool != nullptr); }
		void lock() noexcept;
		void unlock() noexcept;
		static void export_rw_info(Implement::RWPropertyTuple& tuple) noexcept { Implement::ComponentInfoExtractor<CompT...>{}(tuple.components); }

		Implement::ComponentPoolInterface* m_pool = nullptr;
		std::array<std::byte, sizeof(std::shared_lock<std::shared_mutex>) * 2 * sizeof...(CompT)> m_mutex;

		template<typename ...Require> friend struct Implement::SystemStorage;
		template<typename Require> friend struct Implement::FilterAndEventAndSystem;
	};

	template<typename ...CompT> template<typename Func>
	void EntityFilter<CompT...>::operator()(const Entity& wrapper, Func&& f)
	{
		if (wrapper)
		{
			Implement::TypeGroup* group;
			Implement::StorageBlock* block;
			size_t index;
			wrapper.m_imp->read(group, block, index);
			if (group != nullptr)
			{
				auto& infos = Implement::ComponentTypeInfo<CompT...>::info();
				assert(block != nullptr);
				std::array<size_t, sizeof...(CompT)> indexs;
				if (m_pool->loacte_unordered_layouts(group, infos.data(), sizeof...(CompT), indexs.data()))
				{
					std::tuple<std::remove_reference_t<CompT>*...> component_pointer;
					Implement::ComponentTupleHelper<0, sizeof...(CompT)>::translate(block, indexs.data(), component_pointer);
					std::apply([&](auto ...pointer) {
						std::forward<Func>(f)(*pointer...);
					}, component_pointer);
				}
			}
		}
	}

	template<typename ...CompT> void EntityFilter<CompT...>::lock() noexcept
	{
		m_pool->lock(sizeof(std::shared_lock<std::shared_mutex>) * 2, m_mutex.data());
	}

	template<typename ...CompT> void EntityFilter<CompT...>::unlock() noexcept
	{
		m_pool->unlock(sizeof...(CompT), sizeof(std::shared_lock<std::shared_mutex>) * 2, m_mutex.data());
	}

	template<typename ...CompT> struct FilterIterator;

	namespace Implement
	{
		template<typename ...CompT> struct FilterIteratorWrapper
		{
			Entity entity() noexcept { return Entity{ *m_entity }; }
			std::tuple<CompT& ...>& components() noexcept { assert(m_ref.has_value()); return *m_ref; }
			FilterIteratorWrapper() = default;
			FilterIteratorWrapper(const FilterIteratorWrapper&) = default;

			template<size_t index> decltype(auto) get() noexcept { return std::get<index>(*m_ref); }

		private:
			
			void reset(Implement::StorageBlock* input, size_t* layout_index)
			{
				m_entity = input->entitys;
				Implement::ComponentTupleHelper<0, sizeof...(CompT)>::translate(input, layout_index, m_component_pointer);
				m_ref.emplace(std::apply([](auto ...pointer) { return std::tuple<CompT & ...>{*pointer...}; }, m_component_pointer));
			}
			void add()
			{
				m_entity += 1;
				Implement::ComponentTupleHelper<0, sizeof...(CompT)>::add(m_component_pointer);
				m_ref.emplace(std::apply([](auto ...pointer) { return std::tuple<CompT & ...>{*pointer...}; }, m_component_pointer));
			}
			Implement::EntityInterface** m_entity = nullptr;
			std::tuple<CompT* ...> m_component_pointer;
			std::optional<std::tuple<CompT& ...>> m_ref;

			template<typename ...CompT> friend struct FilterIterator;
		};
	}

	template<typename ...CompT> struct Filter;

	template<typename ...CompT> struct FilterIterator
	{
		Implement::FilterIteratorWrapper<CompT...>& operator*() noexcept { return m_wrapper; }
		Implement::FilterIteratorWrapper<CompT...>* operator->() noexcept { return &m_wrapper; }
		bool operator==(const FilterIterator& i) const noexcept {
			return current_block == i.current_block && m_element_last == i.m_element_last;
		}
		bool operator!=(const FilterIterator& i) const noexcept { return !((*this) == i); }
		FilterIterator& operator++() noexcept {
			assert(current_block != nullptr);
			--m_element_last;
			if (m_element_last != 0)
				m_wrapper.add();
			else {
				if (current_block->next != nullptr)
					current_block = current_block->next;
				else {
					++m_buffer_used;
					if (m_buffer_used < m_max_buffer)
						current_block = m_storage_block[m_buffer_used];
					else
						current_block = nullptr;
				}
				if (current_block != nullptr)
				{
					reset_wrapper();
					m_element_last = current_block->available_count;
					assert(*current_block->entitys != nullptr);
				}
				else
					m_element_last = 0;
			}
			return *this;
		}
	private:

		void reset_wrapper() {
			m_wrapper.reset(current_block, m_layout_index + m_buffer_used * sizeof...(CompT));
		}

		Implement::StorageBlock** m_storage_block = nullptr;
		size_t* m_layout_index = nullptr;
		Implement::StorageBlock* current_block = nullptr;
		size_t m_max_buffer = 0;
		size_t m_buffer_used = 0;
		size_t m_element_last = 0;
		Implement::FilterIteratorWrapper<CompT...> m_wrapper;
		template<typename ...CompT> friend struct Filter;
	};

	template<typename ...CompT> struct Filter
	{
		static_assert(Tmp::bool_and<true, Implement::TypePropertyDetector<CompT>::value...>::value, "Filter only accept Type and const Type!");

		FilterIterator<CompT...> begin() noexcept {
			FilterIterator<CompT...> tem;
			tem.m_storage_block = m_storage.data();
			tem.m_layout_index = m_layout_index.data();
			tem.m_max_buffer = m_used_buffer_count;
			if (m_used_buffer_count != 0)
			{
				tem.current_block = m_storage[0];
				tem.m_element_last = tem.current_block->available_count;
				tem.reset_wrapper();
			}
			return tem;
		}
		FilterIterator<CompT...> end() noexcept { FilterIterator<CompT...> tem; return tem; }
		size_t count() const noexcept { return m_total_element_count; }
	protected:
		Filter(Implement::ComponentPoolInterface* pool) noexcept : m_pool(pool) { 
			assert(pool != nullptr); 
			m_storage.resize(10, nullptr);
			m_layout_index.resize(10 * sizeof...(CompT), sizeof...(CompT));
			m_used_buffer_count = 0;
		}
		void lock() noexcept;
		void unlock() noexcept;
		static void export_rw_info(Implement::RWPropertyTuple& tuple) noexcept { Implement::ComponentInfoExtractor<CompT...>{}(tuple.components); }

		std::vector<Implement::StorageBlock*> m_storage;
		std::vector<size_t> m_layout_index;
		size_t m_used_buffer_count;
		std::array<std::byte, sizeof(std::shared_lock<std::shared_mutex>) * 2> m_mutex;
		Implement::ComponentPoolInterface* m_pool = nullptr;
		size_t m_total_element_count;

		template<typename ...Require> friend struct Implement::SystemStorage;
		template<typename Require> friend struct Implement::FilterAndEventAndSystem;
	};

	template<typename ...CompT> void Filter<CompT...>::lock() noexcept
	{
		m_pool->lock(sizeof(std::shared_lock<std::shared_mutex>) * 2, m_mutex.data());
		while (true)
		{
			size_t require_count = m_pool->search_type_group(
				Implement::ComponentTypeInfo<CompT...>::info().data(), sizeof...(CompT),
				m_layout_index.data(), m_storage.data(), m_storage.size(), m_total_element_count
			);
			if (require_count <= m_storage.size())
			{
				m_used_buffer_count = require_count;
				break;
			}
			else {
				m_storage.resize(require_count, nullptr);
				m_layout_index.resize(require_count, sizeof...(CompT));
			}
		}
	}

	template<typename ...CompT> void Filter<CompT...>::unlock() noexcept
	{
		m_pool->unlock(sizeof(std::shared_lock<std::shared_mutex>) * 2, m_mutex.data());
	}

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
			virtual EventPoolMemoryDescription* read_lock(const TypeLayout& layout, size_t mutex_size, void* mutex) noexcept = 0;
			virtual void read_unlock(size_t mutex_size, void* mutex) noexcept = 0;
			virtual EventPoolWriteWrapperInterface* write_lock(const TypeLayout& layout, size_t mutex_size, void* mutex) noexcept = 0;
			virtual void write_unlock(EventPoolWriteWrapperInterface*, size_t mutex_size, void* mutex) noexcept = 0;
		};
	}

	template<typename EventT> struct EventProvider
	{
		static_assert(std::is_same_v<EventT, std::remove_cv_t<std::remove_reference_t<EventT>>>, "EventProvider only accept pure Type!");

		operator bool() const noexcept { return m_ref != nullptr; }
		template<typename ...Parameter> void push(Parameter&& ...pa);
	private:
		EventProvider(Implement::EventPoolInterface* pool) noexcept : m_pool(pool){}
		void lock() noexcept;
		void unlock() noexcept;
		static void export_rw_info(Implement::RWPropertyTuple& tuple) noexcept {
			tuple.events.insert(TypeLayout::create<EventT>());
		}
		Implement::EventPoolWriteWrapperInterface* m_ref = nullptr;
		Implement::EventPoolInterface* m_pool = nullptr;
		std::array<std::byte, sizeof(std::lock_guard<std::mutex>) * 2> m_mutex;
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
				std::apply([&](auto && ...at) {
					new (adress) EventT{ std::forward<decltype(at) &&>(at)... };
					}, po);
			}, &pa_tuple, [](void* in) noexcept { reinterpret_cast<EventT*>(in)->~EventT(); });
	}

	template<typename EventT> void EventProvider<EventT>::lock() noexcept
	{
		m_ref = m_pool->write_lock(TypeLayout::create<EventT>(), sizeof(std::lock_guard<std::mutex>) * 2, m_mutex.data());
		assert(m_ref != nullptr);
	}

	template<typename EventT> void EventProvider<EventT>::unlock() noexcept
	{
		m_pool->write_unlock(m_ref, sizeof(std::lock_guard<std::mutex>) * 2, m_mutex.data());
		m_ref = nullptr;
	}

	template<typename EventT> struct EventViewer
	{
		static_assert(std::is_same_v<EventT, std::remove_cv_t<std::remove_reference_t<EventT>>>, "EventViewer only accept pure Type!");

		struct iterator
		{
			const EventT& operator*() const noexcept{ return *(static_cast<const EventT*>(m_start->event_start) + m_index); }
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
		EventViewer(Implement::EventPoolInterface* pool) noexcept : m_pool(pool){}
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


	namespace Implement
	{

		template<typename Type> struct GobalComponentImp;
		template<typename CompT> using GobalComponentImpPtr = Tool::intrusive_ptr<GobalComponentImp<CompT>>;

		struct GobalComponentInterface
		{
			virtual ~GobalComponentInterface() = default;
			virtual const TypeLayout& layout() const noexcept = 0;
			template<typename T> std::remove_reference_t<T>* get_adress() {
				assert(layout() == TypeLayout::create<T>());
				return static_cast<std::remove_reference_t<T>*>(get_adress_imp());
			}
			virtual void add_ref() const noexcept = 0;
			virtual void sub_ref() const noexcept = 0;
		private:
			virtual void* get_adress_imp() = 0;
		};

		template<typename Type> struct GobalComponentImp : GobalComponentInterface
		{
			operator Type& () { return m_storage; }
			const TypeLayout& layout() const noexcept { return m_layout; }
			virtual void add_ref() const noexcept override { m_ref.add_ref(); }
			virtual void sub_ref() const noexcept override {
				if (m_ref.sub_ref())
					delete this;
			}
			void* get_adress_imp() { return &m_storage; }
			template<typename ...Parameter>
			static Tool::intrusive_ptr<GobalComponentImp> create(Parameter&& ... para) { return new GobalComponentImp{ std::forward<Parameter>(para)... }; }
		private:
			template<typename ...Parameter> GobalComponentImp(Parameter&& ...);
			mutable Tool::atomic_reference_count m_ref;
			const TypeLayout& m_layout;
			Type m_storage;
		};

		template<typename Type> template<typename ...Parameter> GobalComponentImp<Type>::GobalComponentImp(Parameter&& ... para)
			: m_layout(TypeLayout::create<Type>()), m_storage(std::forward<Parameter>(para)...)
		{}

		using GobalComponentInterfacePtr = Tool::intrusive_ptr<GobalComponentInterface>;

		struct GobalComponentPoolInterface
		{
			template<typename Type>
			std::remove_reference_t<Type>* find() noexcept;
			virtual void regedit_gobal_component(GobalComponentInterface*) noexcept = 0;
			virtual void destory_gobal_component(const TypeLayout&) = 0;
		private:
			virtual GobalComponentInterface* find_imp(const TypeLayout& layout) const noexcept = 0;
		};

		template<typename Type> std::remove_reference_t<Type>* GobalComponentPoolInterface::find() noexcept
		{
			using PureType = std::remove_const_t<std::remove_reference_t<Type>>;
			auto re = find_imp(TypeLayout::create<PureType>());
			if (re)
				return re->get_adress<Type>();
			return nullptr;
		}
	}

	template<typename CompT> struct GobalFilter
	{
		static_assert(Implement::TypePropertyDetector<CompT>::value, "GobalFilter only accept Type and const Type!");
		operator bool() const noexcept { return m_cur != nullptr; }
		CompT* operator->() noexcept { return m_cur; }
		CompT& operator*() noexcept { return *m_cur; }
		static void export_rw_info(Implement::RWPropertyTuple& tuple) noexcept
		{
			if constexpr (std::is_const_v<CompT> || std::is_same_v<CompT, std::remove_reference<CompT>>)
				tuple.gobal_components.emplace(TypeLayout::create<CompT>(), Implement::RWProperty::Read);
			else
				tuple.gobal_components[TypeLayout::create<CompT>()] = Implement::RWProperty::Write;
		}
	private:
		void lock() noexcept;
		void unlock()  noexcept { m_cur = nullptr; }
		GobalFilter(Implement::GobalComponentPoolInterface* in) noexcept : m_pool(in), m_cur(nullptr) { assert(m_pool != nullptr); }
		template<typename ...Require> friend struct Implement::SystemStorage;
		template<typename Require> friend struct Implement::FilterAndEventAndSystem;

		Implement::GobalComponentPoolInterface* m_pool = nullptr;
		CompT* m_cur;
	};

	template<typename CompT> void GobalFilter<CompT>::lock() noexcept
	{
		m_cur = m_pool->find<CompT>();
	}

	enum class TickPriority
	{
		HighHigh = 0,
		High = 1,
		Normal = 2,
		Low = 3,
		LowLow = 4,
	};

	enum class TickOrder
	{
		Undefine = 0,
		Mutex = 1,
		Before = 2,
		After = 3,
	};

	namespace Implement
	{
		struct SystemInterface
		{
			virtual void* data() noexcept = 0;
			virtual const TypeLayout& layout() const noexcept = 0;
			virtual void apply(Context*) noexcept = 0;
			virtual void add_ref() noexcept = 0;
			virtual void sub_ref() noexcept = 0;
			virtual TickPriority tick_layout() = 0;
			virtual TickPriority tick_priority() = 0;
			virtual TickOrder tick_order(const TypeLayout&) = 0;
			virtual void rw_property(const TypeLayout*& storage, const RWProperty*& property, const size_t*& count) const noexcept = 0;
		};

		using SystemInterfacePtr = Tool::intrusive_ptr<SystemInterface>;

		struct TemplateSystemInterface
		{
			virtual void apply(Context*) noexcept = 0;
			virtual void add_ref() const noexcept = 0;
			virtual void sub_ref() const noexcept = 0;
		};

		using TemplateSystemInterfacePtr = Tool::intrusive_ptr<TemplateSystemInterface>;

		struct SystemPoolInterface
		{
			virtual SystemInterface* find_system(const TypeLayout& ti) noexcept = 0;
			virtual void regedit_system(SystemInterface*) noexcept = 0;
			virtual void destory_system(const TypeLayout& id) noexcept = 0;
			virtual void regedit_template_system(TemplateSystemInterface*) noexcept = 0;
		};

	}

	template<typename Type>
	struct SystemFilter
	{
		static_assert(Implement::TypePropertyDetector<Type>::value, "SystemWrapper only accept Type and const Type!");
		operator bool() const noexcept { return m_resource != nullptr; }
		Type* operator->() noexcept { return m_resource; }
	private:
		SystemFilter(Implement::SystemPoolInterface* pool) noexcept : m_pool(pool), m_resource(nullptr){}
		static void export_rw_info(Implement::RWPropertyTuple& tuple) noexcept;
		void lock() noexcept;
		void unlock()  noexcept { m_resource = nullptr; }
		Type* m_resource;
		Implement::SystemPoolInterface* m_pool;
		template<typename ...Require> friend struct Implement::SystemStorage;
		template<typename Require> friend struct Implement::FilterAndEventAndSystem;
	};

	template<typename Type>
	void SystemFilter<Type>::export_rw_info(Implement::RWPropertyTuple& tuple) noexcept
	{
		if constexpr (std::is_const_v<Type>)
			tuple.systems.emplace(TypeLayout::create<Type>(), Implement::RWProperty::Read);
		else
			tuple.systems[TypeLayout::create<Type>()] = Implement::RWProperty::Write;
	}

	template<typename Type> void SystemFilter<Type>::lock() noexcept
	{
		//Implement::SystemPoolInterface& SI = *in;
		auto ptr = m_pool->find_system(TypeLayout::create<Type>());
		if (ptr)
			m_resource = reinterpret_cast<Type*>(ptr->data());
		else
			m_resource = nullptr;
	}


	namespace Implement
	{

		template<typename Type> struct ContextStorage
		{
			bool lock() noexcept { return true; }
			void unlock() {  }
			Context* as_pointer() noexcept { return m_ref; }
			static void export_rw_info(Implement::RWPropertyTuple& tuple) noexcept{}
			ContextStorage(Context* input) noexcept : m_ref(input) {}
		private:
			using PureType = std::remove_const_t<Type>;
			static_assert(std::is_same_v<PureType, Context&>, "System require Parameter Should be \"Context&\" but not \"Context\"");
			Context* m_ref = nullptr;
		};

		template<typename Type> struct FilterAndEventAndSystem
		{
			void lock() noexcept { m_storage.lock(); }
			void unlock() noexcept { m_storage.unlock(); }
			std::remove_reference_t<Type>* as_pointer() noexcept { return &m_storage; }
			static void export_rw_info(Implement::RWPropertyTuple& tuple) noexcept { PureType::export_rw_info(tuple); }
			FilterAndEventAndSystem(Context* in) noexcept : m_storage(*in) {}
		private:
			using PureType = std::remove_const_t<std::remove_reference_t<Type>>;
			static_assert(
				std::is_reference_v<Type>,
				"System require Parameter Like Event And Filter should be \'Type&\' or \'const Type&\' bug not \'Type\'"
				);
			PureType m_storage;
		};

		template<typename T> struct IsContext : std::false_type {};
		template<> struct IsContext<Context> : std::true_type {};

		template<typename T> struct IsFilterOrEventOrSystem : std::false_type {};
		template<typename ...T> struct IsFilterOrEventOrSystem<Filter<T...>> : std::true_type {};
		template<typename ...T> struct IsFilterOrEventOrSystem<EntityFilter<T...>> : std::true_type {};
		template<typename T> struct IsFilterOrEventOrSystem<EventProvider<T>> : std::true_type {};
		template<typename T> struct IsFilterOrEventOrSystem<EventViewer<T>> : std::true_type {};
		template<typename T> struct IsFilterOrEventOrSystem<SystemFilter<T>> : std::true_type {};
		template<typename T> struct IsFilterOrEventOrSystem<GobalFilter<T>> : std::true_type {};

		template<size_t index, typename InputType> struct SystemStorageDetectorImp { 
			static_assert(index != 0, "unsupport filter");
		};
		template<typename InputType> struct SystemStorageDetectorImp<1, InputType> { using Type = ContextStorage<InputType>; };
		template<typename InputType> struct SystemStorageDetectorImp<2, InputType> { using Type = FilterAndEventAndSystem<InputType>; };

		template<typename InputType> struct SystemStorageDetector
		{
			using PureType = std::remove_const_t<std::remove_reference_t<InputType>>;
			using Type = typename SystemStorageDetectorImp<
				IsContext<PureType>::value ? 1 : (IsFilterOrEventOrSystem<PureType>::value ? 2 : 0),
				InputType
			>::Type;
		};

		template<size_t start, size_t end> struct SystemStorageImp
		{
			template<typename Tuple>
			static void lock(Tuple& tuple) noexcept
			{
				std::get<start>(tuple).lock();
				SystemStorageImp<start + 1, end>::lock(tuple);
			}
			template<typename Tuple>
			static void unlock(Tuple& tuple) noexcept
			{
				std::get<start>(tuple).unlock();
				SystemStorageImp<start + 1, end>::unlock(tuple);
			}
		};

		template<size_t end> struct SystemStorageImp<end, end>
		{
			template<typename Tuple>
			static void lock(Tuple& tuple) noexcept {  }
			template<typename Tuple>
			static void unlock(Tuple& tuple) noexcept {}
		};

		template<typename ...Require>
		struct SystemRWInfo
		{
			SystemRWInfo();
			void rw_property(const TypeLayout*& layout, const RWProperty*& property, const size_t*& index) const noexcept
			{
				layout = m_rw_type.data(); property = m_rw_property.data(); index = m_index.data();
			}
		private:
			std::vector<TypeLayout> m_rw_type;
			std::vector<RWProperty> m_rw_property;
			std::array<size_t, 4> m_index;
		};

		template<typename ...Type> struct SystemRWInfoImp { void operator()(Implement::RWPropertyTuple& tuple) {} };
		template<typename Cur, typename ...Type> struct SystemRWInfoImp<Cur, Type...> { void operator()(Implement::RWPropertyTuple& tuple) {
			SystemStorageDetector<Cur>::Type::export_rw_info(tuple);
			SystemRWInfoImp<Type...>{}(tuple);
		} };

		template<typename ...Require> SystemRWInfo<Require...>::SystemRWInfo()
		{
			Implement::RWPropertyTuple tuple;
			SystemRWInfoImp<Require...>{}(tuple);
			m_index[0] = tuple.systems.size();
			m_index[1] = tuple.gobal_components.size();
			m_index[2] = tuple.components.size();
			m_index[3] = tuple.events.size();
			size_t total_size = m_index[0] + m_index[1] + m_index[2] + m_index[3];
			m_rw_type.reserve(total_size);
			m_rw_property.reserve(total_size);
			for (auto& ite : tuple.systems)
			{
				m_rw_type.push_back(ite.first);
				m_rw_property.push_back(ite.second);
			}
			for (auto& ite : tuple.gobal_components)
			{
				m_rw_type.push_back(ite.first);
				m_rw_property.push_back(ite.second);
			}
			for (auto& ite : tuple.components)
			{
				m_rw_type.push_back(ite.first);
				m_rw_property.push_back(ite.second);
			}
			for (auto& ite : tuple.events)
			{
				m_rw_type.push_back(ite);
				m_rw_property.push_back(RWProperty::Write);
			}
		}

		template<size_t index> struct CopyContext
		{
			auto operator()(Context* in) { return std::tuple_cat(CopyContext<index - 1>{}(in), std::make_tuple(in)); }
		};

		template<> struct CopyContext<0>
		{
			std::tuple<> operator()(Context* in) { return std::tuple<>{}; }
		};

		template<typename ...Requires> struct SystemStorage
		{
			template<typename System>
			void apply(System& sys, Context* con) noexcept
			{
				SystemStorageImp<0, sizeof...(Requires)>::lock(m_storage);
				std::apply([&](auto && ... ai) { sys(*ai.as_pointer()...); }, m_storage);
				SystemStorageImp<0, sizeof...(Requires)>::unlock(m_storage);
			}
			static SystemRWInfo<Requires...> rw_info;
			SystemStorage(Context* in) : m_storage(CopyContext<sizeof...(Requires)>{}(in)) {}
		private:
			std::tuple<typename SystemStorageDetector<Requires>::Type...> m_storage;
		};

		template<typename ...Requires> SystemRWInfo<Requires...> SystemStorage<Requires...>::rw_info;
	}

	namespace Implement
	{

		template<typename SystemT, typename = std::void_t<>>
		struct TickLayoutDetector
		{
			TickPriority operator()(SystemT& sys) const { return TickPriority::Normal; }
		};

		template<typename SystemT>
		struct TickLayoutDetector<SystemT, std::void_t<std::enable_if_t<std::is_same_v<typename Tmp::function_type_extractor<decltype(&SystemT::tick_layout)>::pure_type, TickPriority()>>>>
		{
			TickPriority operator()(SystemT& sys) const { return sys.tick_layout(); }
		};

		template<typename SystemT, typename = std::void_t<>>
		struct TickPriorityDetector
		{
			TickPriority operator()(SystemT& sys) const { return TickPriority::Normal; }
		};

		template<typename SystemT>
		struct TickPriorityDetector<SystemT, std::void_t<std::enable_if_t<std::is_same_v<typename Tmp::function_type_extractor<decltype(&SystemT::tick_priority)>::pure_type, TickPriority()>>>>
		{
			TickPriority operator()(SystemT& sys) const { return sys.tick_priority(); }
		};

		template<typename SystemT, typename = std::void_t<>>
		struct TickOrderDetector
		{
			TickOrder operator()(SystemT& sys, const TypeLayout& id) const { return TickOrder::Undefine; }
		};

		template<typename SystemT>
		struct TickOrderDetector<SystemT, std::enable_if_t<std::is_same_v<typename Tmp::function_type_extractor<decltype(&SystemT::tick_order)>::pure_type, TickOrder(const TypeLayout&)>>>
		{
			TickOrder operator()(SystemT& sys, const TypeLayout& id) const { return sys.tick_order(id); }
		};

		template<typename Type> struct SystemImplement : SystemInterface
		{
			virtual void* data() noexcept { return &m_storage; }
			using PureType = std::remove_const_t<std::remove_reference_t<Type>>;
			using ParameterType = Tmp::function_type_extractor<PureType>;
			using AppendType = typename ParameterType::template extract_parameter<SystemStorage>;
			virtual void apply(Context* con) noexcept override { 
				m_append_storgae.apply(m_storage, con); 
			}
			virtual void add_ref() noexcept { m_ref.add_ref(); };
			virtual void sub_ref() noexcept {
				if (m_ref.sub_ref())
					m_deconstructor(this);
			}
			virtual const TypeLayout& layout() const noexcept override { return TypeLayout::create<Type>(); }
			template<typename ...Parameter> SystemImplement(
				Context* in, void (*deconstructor)(const SystemImplement<Type>*) noexcept, 
				TickPriority prioerity, TickPriority layout,
				Parameter&& ... para
			);
			virtual TickPriority tick_layout() override { 
				return (m_outside_layout == TickPriority::Normal) ? TickLayoutDetector<PureType>{}(m_storage) : m_outside_layout;
			}
			virtual TickPriority tick_priority() override { 
				return (m_outside_priority == TickPriority::Normal) ? TickPriorityDetector<PureType>{}(m_storage) : m_outside_priority;
			}
			virtual TickOrder tick_order(const TypeLayout& layout) override { return TickOrderDetector<PureType>{}(m_storage, layout); };
			virtual void rw_property(const TypeLayout*& storage, const RWProperty*& property, const size_t*& count) const noexcept override { 
				AppendType::rw_info.rw_property(storage, property, count);
			}
			operator std::remove_reference_t<Type>& () noexcept { return m_storage; }
		private:
			//virtual std::type_index id() const noexcept { return typeid(Type); }
			Tool::atomic_reference_count m_ref;
			PureType m_storage;
			AppendType m_append_storgae;
			TickPriority m_outside_priority;
			TickPriority m_outside_layout;
			void (*m_deconstructor)(const SystemImplement<Type>*) noexcept;
		};

		template<typename Type> template<typename ...Parameter>
		SystemImplement<Type>::SystemImplement(
			Context* in, void (*deconstructor)(const SystemImplement<Type>*) noexcept, 
			TickPriority prioerity, TickPriority layout,
			Parameter&& ... para
		)
			: m_storage(std::forward<Parameter>(para)...), m_append_storgae(in), m_deconstructor(deconstructor), 
			m_outside_priority(prioerity), m_outside_layout(layout)
		{
			assert(m_deconstructor != nullptr);
		}

	}

	namespace Implement
	{
		struct AsynchronousWorkInterface
		{
			virtual void add_ref() noexcept = 0;
			virtual void sub_ref() noexcept = 0;
			virtual bool apply(Context&) noexcept = 0;
			virtual ~AsynchronousWorkInterface() = default;
		};

		template<typename CallableObject, typename ...Parameter> struct AsynchronousWorkImplement 
			: AsynchronousWorkInterface
		{

			virtual void add_ref() noexcept { m_ref.add_ref(); }
			virtual void sub_ref() noexcept { 
				if (m_ref.sub_ref())
					delete this;
			}

			virtual bool apply(Context& context) noexcept {
				if constexpr (std::is_same_v<decltype(apply_imp(context)), void>)
				{
					apply_imp(context);
					return false;
				}
				else {
					bool result = apply_imp(context);
					return result;
				}
			}

			decltype(auto) apply_imp(Context& context)
			{
				return std::apply([&, this](auto&& ...at) {
					return std::forward<CallableObject>(object)(context, std::forward<decltype(at) &&>(at)...);
				}, parameter);
			}


			std::remove_reference_t<CallableObject> object;
			std::tuple<std::remove_reference_t<Parameter>...> parameter;
			Tool::atomic_reference_count m_ref;

			AsynchronousWorkImplement(CallableObject&& object, Parameter&& ... pa)
				: object(std::forward<CallableObject>(object)), parameter(std::forward<Parameter>(pa)...) {}

		};
	}

	struct Context
	{
		Entity create_entity() { return Entity{ create_entity_imp() }; }
		template<typename CompT, typename ...Parameter> std::remove_reference_t<std::remove_const_t<CompT>>& create_component(Entity entity, Parameter&& ...p);
		template<typename CompT, typename ...Parameter> std::remove_reference_t<std::remove_const_t<CompT>>& create_gobal_component(Parameter&& ...p);
		template<typename SystemT, typename ...Parameter> std::remove_reference_t<std::remove_const_t<SystemT>>& create_system(Parameter&& ...p);
		template<typename SystemT> void create_system(SystemT&& p, TickPriority priority = TickPriority::Normal, TickPriority layout = TickPriority::Normal);
		template<typename SystemT> void destory_system();
		template<typename CompT> bool destory_component(Entity entity);
		template<typename CompT> void destory_gobal_component();
		void destory_entity(Entity entity) { 
			assert(entity);
			Implement::ComponentPoolInterface* CPI = *this;
			assert(entity); 
			CPI->entity_destory(entity.m_imp);
		}
		virtual void exit() noexcept = 0;
		virtual float duration_s() const noexcept = 0;
		template<typename CallableObject, typename ...Parameter> void insert_asynchronous_work(CallableObject&& co, Parameter&& ... pa);
	private:
		virtual void insert_asynchronous_work_imp(Implement::AsynchronousWorkInterface* ptr) = 0;
		template<typename CompT> friend struct Implement::FilterAndEventAndSystem;
		template<typename CompT> friend struct Implement::ContextStorage;
		virtual operator Implement::ComponentPoolInterface* () = 0;
		virtual operator Implement::GobalComponentPoolInterface* () = 0;
		virtual operator Implement::EventPoolInterface* () = 0;
		virtual operator Implement::SystemPoolInterface* () = 0;
		virtual Implement::EntityInterfacePtr create_entity_imp() = 0;
	};

	template<typename CallableObject, typename ...Parameter> void Context::insert_asynchronous_work(CallableObject&& co, Parameter&& ... pa)
	{
		Tool::intrusive_ptr<Implement::AsynchronousWorkInterface> ptr = new Implement::AsynchronousWorkImplement<CallableObject, Parameter...>{ 
			std::forward<CallableObject>(co), std::forward<Parameter>(pa)...  
		};
		insert_asynchronous_work_imp(ptr);
	}

	template<typename CompT> void Context::destory_gobal_component()
	{
		Implement::GobalComponentPoolInterface* GPI = *this;
		GPI->destory_gobal_component(TypeLayout::create<CompT>());
	}

	template<typename CompT, typename ...Parameter> std::remove_reference_t<std::remove_const_t<CompT>>& Context::create_component(Entity entity, Parameter&& ...p)
	{
		Implement::ComponentPoolInterface* cp = *this;
		assert(entity);
		return cp->construction_component<CompT>(entity.m_imp, std::forward<Parameter>(p)...);
	}

	template<typename CompT> bool Context::destory_component(Entity entity)
	{
		Implement::ComponentPoolInterface* cp = *this;
		assert(entity);
		return cp->deconstruct_component(entity.m_imp, TypeLayout::create<CompT>());
	}

	template<typename CompT, typename ...Parameter> std::remove_reference_t<std::remove_const_t<CompT>>& Context::create_gobal_component(Parameter&& ...p)
	{
		Implement::GobalComponentPoolInterface* cp = *this;
		auto result = Implement::GobalComponentImp<CompT>::create(std::forward<Parameter>(p)...);
		cp->regedit_gobal_component(result);
		return *result;
	}

	template<typename SystemT, typename ...Parameter> std::remove_reference_t<std::remove_const_t<SystemT>>& Context::create_system(Parameter&& ...p)
	{
		Implement::SystemPoolInterface* SI = *this;
		Tool::intrusive_ptr<Implement::SystemImplement<SystemT>> ptr = new Implement::SystemImplement<SystemT>{this, [](const Implement::SystemImplement<SystemT> * in) noexcept {delete in; }, TickPriority::Normal, TickPriority::Normal, std::forward<Parameter>(p)... };
		SI->regedit_system(ptr);
		return *ptr;
	}

	template<typename SystemT> void Context::create_system(SystemT&& p, TickPriority priority, TickPriority layout)
	{
		Implement::SystemPoolInterface* SI = *this;
		Tool::intrusive_ptr<Implement::SystemImplement<SystemT>> ptr = new Implement::SystemImplement<SystemT>{this, [](const Implement::SystemImplement<SystemT> * in) noexcept {delete in; }, priority, layout, std::forward<SystemT>(p) };
		SI->regedit_system(ptr);
	}

	template<typename SystemT> void Context::destory_system()
	{
		Implement::SystemPoolInterface* SI = *this;
		SI->destory_system(TypeLayout::create<SystemT>());
	}

}

namespace std
{
	template<typename ...AT> struct tuple_size<typename Noodles::Implement::FilterIteratorWrapper<AT...>> : std::integral_constant<size_t, sizeof...(AT)> {};
	template<size_t index, typename ...AT> decltype(auto) get(typename Noodles::Implement::FilterIteratorWrapper<AT...>& ite) {
		return std::get<index>(ite.components());
	}
	template<size_t index, typename ...AT> struct tuple_element<index, typename Noodles::Implement::FilterIteratorWrapper<AT...>> : std::tuple_element<index, std::tuple<AT...>> {};
}