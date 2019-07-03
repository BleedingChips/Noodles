#pragma once
#include "aid.h"
#include "entity_interface.h"
namespace Noodles
{

	namespace Implement
	{
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
	}

	namespace Implement
	{
		enum class EntityOperator : uint8_t
		{
			Construct = 0,
			Destruct = 1,
			DeleteAll = 2,
			Destory = 3
		};

		struct ComponentPoolInterface
		{
			template<typename CompT, typename ...Parameter> CompT& construction_component(EntityInterface* owner, Parameter&& ...pa);

			virtual size_t type_group_count() const noexcept = 0;
			virtual size_t search_type_group(
				const TypeInfo* require_tl, size_t require_tl_count,
				TypeGroup** output_tg,
				size_t* output_tl_index,
				size_t* output_tg_used_index
			) const noexcept = 0;

			virtual void find_top_block(const TypeGroup** tg, StorageBlock** output, size_t length) const noexcept = 0;

			virtual bool loacte_unordered_layouts(const TypeGroup* input, const TypeInfo* require_layout, size_t index, size_t* output) = 0;
			virtual void construct_component(const TypeInfo& layout, void(*constructor)(void*, void*), void* data, EntityInterface*, void(*deconstructor)(void*) noexcept, void(*mover)(void*, void*) noexcept) = 0;
			virtual void deconstruct_component(EntityInterface*, const TypeInfo& layout) noexcept = 0;
			virtual void handle_entity_imp(EntityInterface*, EntityOperator ope) noexcept = 0;
			void entity_destory(EntityInterface* in) { return handle_entity_imp(in, EntityOperator::Destory); }
			void entity_delete_all(EntityInterface* in) { return handle_entity_imp(in, EntityOperator::DeleteAll); }
		};

		template<typename CompT, typename ...Parameter> auto ComponentPoolInterface::construction_component(EntityInterface* owner, Parameter&& ...pa) -> CompT &
		{
			CompT* result = nullptr;
			auto pa_tuple = std::forward_as_tuple(result, std::forward<Parameter>(pa)...);
			construct_component(TypeLayout::create<CompT>(), [](void* adress, void* para) {
				auto& ref = *static_cast<decltype(pa_tuple)*>(para);
				using Type = CompT;
				std::apply([&](auto& ref, auto&& ...at) { ref = new (adress) Type{ std::forward<decltype(at) &&>(at)... }; }, ref);
			}, &pa_tuple, owner, [](void* in) noexcept { static_cast<CompT*>(in)->~CompT(); }, [](void* target, void* source) noexcept {
				new (target) CompT{ std::move(*reinterpret_cast<CompT*>(source)) };
			});
			return *result;
		}

	}

	namespace Implement
	{
		template<typename Type> struct FilterAndEventAndSystem;
	}

	template<typename ...CompT> struct FilterIterator;

	namespace Implement
	{
		template<typename ...CompT> struct FilterIteratorWrapper
		{
			Entity entity() noexcept { return Entity{ *m_entity }; }
			std::tuple<CompT& ...>& components() noexcept { assert(m_ref.has_value()); return *m_ref; }

			
			FilterIteratorWrapper(const FilterIteratorWrapper&) = default;
			FilterIteratorWrapper() = default;

			template<size_t index> decltype(auto) get() noexcept { return std::get<index>(*m_ref); }

		private:

			void set(Implement::EntityInterface* e_interface, std::tuple<CompT* ...>& pointer)
			{
				m_entity = e_interface;
				m_ref.emplace(std::apply([](auto ...pointer) { return std::tuple<CompT & ...>{*pointer...}; }, pointer));
			}
			Implement::EntityInterface* m_entity = nullptr;
			std::optional<std::tuple<CompT& ...>> m_ref;
			template<typename ...CompT> friend struct FilterIterator;
		};
	}

	template<typename ...CompT> struct FilterIterator
	{
		Implement::FilterIteratorWrapper<CompT...>& operator*() noexcept { return m_wrapper; }
		Implement::FilterIteratorWrapper<CompT...>* operator->() noexcept { return &m_wrapper; }
		bool operator==(const FilterIterator& i) const noexcept {
			return current_block == i.current_block && m_element_last == i.m_element_last;
		}
		bool operator!=(const FilterIterator& i) const noexcept { return !((*this) == i); }
		FilterIterator& operator++() noexcept;

		FilterIterator(const FilterIterator&) = default;
		FilterIterator(Implement::StorageBlock** storage_buffer = nullptr, size_t* type_info = nullptr, size_t storage_buffer_count = 0) noexcept;

	private:

		Implement::StorageBlock** m_storage_block = nullptr;
		size_t* m_layout_index = nullptr;
		Implement::StorageBlock* m_current_block = nullptr;
		Implement::EntityInterface** m_entity_start = nullptr;
		size_t m_storage_block_count = 0;
		size_t m_current_storage_block_index = 0;
		size_t m_element_last = 0;
		std::tuple<CompT* ...> m_pointer;
		Implement::FilterIteratorWrapper<CompT...> m_wrapper;
		template<typename ...CompT> friend struct Filter;
	};

	template<typename ...CompT> auto FilterIterator<CompT...>::operator++() noexcept ->FilterIterator&
	{
		assert(m_current_block != nullptr);
		--m_element_last;
		if (m_element_last != 0)
		{
			++m_entity_start;
			Implement::ComponentTupleHelper<CompT...>::add(m_pointer);
			m_wrapper.set(*m_entity_start, m_pointer);
		}
		else {
			if (m_current_block->next != nullptr)
				m_current_block = m_current_block->next;
			else {
				for (++m_current_storage_block_index; m_current_storage_block_index < m_storage_block_count; ++m_current_storage_block_index)
				{
					if (m_storage_block[m_current_storage_block_index] != nullptr)
					{
						m_current_block = m_storage_block[m_current_storage_block_index];
						break;
					}
				}
				if (m_current_storage_block_index == m_storage_block_count)
					m_current_block = nullptr;
			}
			if (m_current_block != nullptr)
			{
				m_entity_start = m_current_block->entitys();
				Implement::ComponentTupleHelper<0. sizeof...(CompT)>::translate(m_current_block, m_layout_index + sizeof...(CompT) * m_current_storage_block_index);
				m_wrapper.set(*m_entity_start, m_pointer);
				m_element_last = current_block->available_count;
				assert(*current_block->entitys != nullptr);
			}
			else
				m_element_last = 0;
		}
		return *this;
	}

	template<typename ...CompT> FilterIterator<CompT...>::FilterIterator(Implement::StorageBlock** storage_buffer, size_t* type_info, size_t storage_buffer_count) noexcept
		: m_storage_block(storage_buffer), m_layout_index(type_info), m_storage_block_count(storage_buffer_count)
	{
		if(storage_buffer_count > 0 && storage_buffer != nullptr)
		{
			for (; m_current_storage_block_index < m_storage_block_count && m_current_block != nullptr; ++m_current_storage_block_index)
				m_current_block = storage_buffer[m_current_storage_block_index];
			if (m_current_block != nullptr)
			{
				m_entity_start = m_current_block->entitys();
				Implement::ComponentTupleHelper<0. sizeof...(CompT)>::translate(m_current_block, m_layout_index + sizeof...(CompT) * m_current_storage_block_index);
				m_wrapper.set(*m_entity_start, m_pointer);
				m_element_last = current_block->available_count;

			}
		}
	}

	template<typename ...CompT> struct Filter
	{
		static_assert(Potato::Tmp::bool_and<true, Implement::AcceptableTypeDetector<CompT...>::value...>::value, "Filter only accept Type and const Type!");

		FilterIterator<CompT...> begin() noexcept {
			return FilterIterator<CompT...>{ m_start , m_tl_index_start , m_type_group_count };
		}
		FilterIterator<CompT...> end() noexcept { return FilterIterator<CompT...>{}; }
		size_t count() const noexcept { return m_total_element_count; }

	protected:

		Filter(Implement::ComponentPoolInterface* pool) noexcept : m_pool(pool) {}

		void type_group_change() noexcept;
		void system_change() noexcept {};
		void gobal_component_change() noexcept {}

		void pre_apply() noexcept;
		void pos_apply() noexcept {}

		static void export_rw_info(Implement::ReadWritePropertyMap& tuple) noexcept { Implement::TypeInfoListExtractor<CompT...>{}(tuple.components); }
		void export_type_group_used(Implement::ReadWriteProperty*) const noexcept;


		std::vector<Implement::TypeGroup*> m_matching_type_group;
		std::vector<Implement::StorageBlock*> m_top_block;
		std::vector<size_t> m_type_ref;
		std::vector<size_t> m_used_type_group;
		size_t m_total_element_count;

		Implement::ComponentPoolInterface* m_pool = nullptr;

		template<typename ...Require> friend struct Implement::SystemStorage;
		template<typename Require> friend struct Implement::FilterAndEventAndSystem;
	};

	template<typename ...CompT> void Filter<CompT...>::type_group_change() noexcept
	{
		assert(m_pool != nullptr);
		size_t count = m_pool->type_group_count();
		m_matching_type_group.resize(count);
		m_top_block.resize(count);
		m_type_ref.resize(sizeof...(CompT) * count);
		m_used_type_group.resize(count);
		count = m_pool->search_type_group(Implement::TypeInfoList<CompT>::info(), sizeof...(CompT), m_matching_type_group.data(), m_type_ref.data(), m_used_type_group.data());
		m_matching_type_group.resize(count);
		m_top_block.resize(count);
		m_type_ref.resize(sizeof...(CompT) * count);
		m_used_type_group.resize(count);
	}

	template<typename ...CompT> void Filter<CompT...>::pre_apply() noexcept
	{
		assert(m_pool != nullptr);
		m_pool->find_top_block(m_matching_type_group.data(), m_top_block.data(), m_top_block.size());
	}

	template<typename ...CompT> void Filter<CompT...>::export_type_group_used(Implement::ReadWriteProperty* mapping) const noexcept
	{
		for (auto ite : m_used_type_group)
		{
			if constexpr (Implement::TypeInfoList<CompT>::is_pure)
				mapping[ite] = Implement::ReadWriteProperty::Write;
			else{
				auto& ref = mapping[ite];
				if (ref == Implement::ReadWriteProperty::Unknow)
					ref = Implement::ReadWriteProperty::Read;
			}
		}
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
					std::tuple<std::remove_reference_t<CompT>* ...> component_pointer;
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

}