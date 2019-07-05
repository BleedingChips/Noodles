#pragma once
#include "aid.h"
#include "entity_interface.h"
#include <optional>
#include "..//..//Potato/tmp.h"
namespace Noodles
{

	namespace Implement
	{
		template<size_t start, size_t end> struct ComponentTupleHelper
		{
			template<typename TupleType>
			static void translate(StorageBlock const* block, const size_t* index, TupleType& tuple) {
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
			static void translate(const StorageBlock* block, const size_t* index, TupleType& tuple) {}

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
			virtual void search_type_group(
				const TypeInfo* require_tl, size_t require_tl_count,
				TypeGroup** output_tg,
				size_t* output_tl_index
			) const noexcept = 0;

			virtual size_t find_top_block(TypeGroup ** tg, StorageBlock ** output, size_t length) const noexcept = 0;
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
			construct_component(TypeInfo::create<CompT>(), [](void* adress, void* para) {
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
			Entity entity() noexcept { return Entity{ m_entity }; }
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
			return m_current_block == i.m_current_block && m_element_last == i.m_element_last;
		}
		bool operator!=(const FilterIterator& i) const noexcept { return !((*this) == i); }
		FilterIterator& operator++() noexcept;

		FilterIterator(const FilterIterator&) = default;
		FilterIterator(Implement::StorageBlock ** storage_buffer = nullptr, size_t* type_info = nullptr, size_t storage_buffer_count = 0) noexcept;

	private:

		Implement::StorageBlock ** m_storage_block = nullptr;
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
			Implement::ComponentTupleHelper<0, sizeof...(CompT)>::add(m_pointer);
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
				m_entity_start = m_current_block->entitys;
				Implement::ComponentTupleHelper<0, sizeof...(CompT)>::translate(m_current_block, m_layout_index + sizeof...(CompT) * m_current_storage_block_index, m_pointer);
				m_wrapper.set(*m_entity_start, m_pointer);
				m_element_last = m_current_block->available_count;
				assert(*m_current_block->entitys != nullptr);
			}
			else
				m_element_last = 0;
		}
		return *this;
	}

	template<typename ...CompT> FilterIterator<CompT...>::FilterIterator(Implement::StorageBlock ** storage_buffer, size_t* type_info, size_t storage_buffer_count) noexcept
		: m_storage_block(storage_buffer), m_layout_index(type_info), m_storage_block_count(storage_buffer_count)
	{
		if(storage_buffer_count > 0 && storage_buffer != nullptr)
		{
			for (; m_current_storage_block_index < m_storage_block_count && m_current_block != nullptr; ++m_current_storage_block_index)
				m_current_block = storage_buffer[m_current_storage_block_index];
			if (m_current_block != nullptr)
			{
				m_entity_start = m_current_block->entitys;
				Implement::ComponentTupleHelper<0, sizeof...(CompT)>::translate(m_current_block, m_layout_index + sizeof...(CompT) * m_current_storage_block_index, m_pointer);
				m_wrapper.set(*m_entity_start, m_pointer);
				m_element_last = m_current_block->available_count;
			}
		}
	}

	namespace Implement
	{
		template<typename ...CompT>
		struct FilterBase
		{
			void envirment_change(bool system, bool gobalcomponent, bool component);
			static void export_rw_info(Implement::ReadWritePropertyMap& tuple) noexcept { Implement::TypeInfoListExtractor<CompT...>{}(tuple.components); }
			void export_type_group_used(const TypeInfo* conflig_type, size_t conflig_count, Implement::ReadWriteProperty*) const noexcept;
			FilterBase(Implement::ComponentPoolInterface* ptr) noexcept : m_pool(ptr) { assert(m_pool); }
			size_t update_component(std::vector<StorageBlock*>& p) {
				p.resize(m_type_group_count);
				return 	m_pool->find_top_block(m_all_type_group.data(), p.data(), m_type_group_count);
			}
			size_t* layout_index() noexcept { return m_type_layout_index.data(); }
			size_t type_group_count() const noexcept { return m_type_group_count; }
			size_t* find_type_layout(const Implement::TypeGroup* input) const noexcept {
				assert(input != nullptr);
				for (size_t i = 0; i < m_type_group_count; ++i)
				{
					if (m_all_type_group[i] == input)
						return m_type_layout_index.data() + sizeof...(CompT) * i;
				}
				return nullptr;
			}
		private:
			observer_ptr<Implement::ComponentPoolInterface> m_pool;
			std::vector<TypeGroup*> m_all_type_group;
			std::vector<size_t> m_type_layout_index;
			size_t m_type_group_count = 0;
		};

		template<typename ...CompT> void FilterBase<CompT...>::export_type_group_used(const TypeInfo* conflig_type, size_t conflig_count, Implement::ReadWriteProperty* mapping) const noexcept
		{
			using Infos = Implement::TypeInfoList<CompT...>;
			bool find = false;
			for (size_t i = 0; i < conflig_count; ++i)
			{
				for (size_t k = 0; k < Infos::info().size(); ++k)
				{
					if (Infos::info()[k] == conflig_type[i])
					{
						find = true;
						goto next;
					}
				}
			}
		next:
			if (find)
			{
				size_t index = 0;
				for (size_t index = 0; index < m_type_group_count; ++index)
				{
					if (m_all_type_group[index] != nullptr)
					{
						if constexpr (Potato::Tmp::bool_or<false, Implement::AcceptableTypeDetector<CompT>::is_pure...>::value)
							mapping[index] = Implement::ReadWriteProperty::Write;
						else {
							auto& ref = mapping[index];
							if (ref == Implement::ReadWriteProperty::Unknow)
								ref = Implement::ReadWriteProperty::Read;
						}
					}
				}
			}
		}

		template<typename ...CompT> void FilterBase<CompT...>::envirment_change(bool system, bool gobalcomponent, bool component)
		{
			if (component)
			{
				using Infos = Implement::TypeInfoList<CompT...>;
				m_type_group_count = m_pool->type_group_count();
				m_all_type_group.resize(m_type_group_count);
				m_type_layout_index.resize(m_type_group_count * sizeof...(CompT));
				m_pool->search_type_group(
					Infos::info().data(),
					Infos::info().size(),
					m_all_type_group.data(),
					m_type_layout_index.data()
				);
			}
		}
	}

	template<typename ...CompT> struct Filter : protected Implement::FilterBase<CompT...>
	{
		using Super = Implement::FilterBase<CompT...>;

		static_assert(Potato::Tmp::bool_and<true, Implement::AcceptableTypeDetector<CompT>::value...>::value, "Filter only accept Type and const Type!");

		FilterIterator<CompT...> begin() noexcept {
			return FilterIterator<CompT...>{ m_top_block.data(), Super::layout_index(), Super::type_group_count() };
		}
		FilterIterator<CompT...> end() noexcept { return FilterIterator<CompT...>{}; }
		size_t count() const noexcept { return m_total_element_count; }

	protected:

		Filter(Implement::ComponentPoolInterface* pool) noexcept : Implement::FilterBase<CompT...>(pool) {}

		void pre_apply() noexcept { m_total_element_count = Super::update_component(m_top_block); }
		void pos_apply() noexcept {}

		std::vector<Implement::StorageBlock*> m_top_block;
		size_t m_total_element_count;

		template<typename Require> friend struct Implement::FilterAndEventAndSystem;
	};

	template<typename ...CompT> struct EntityFilter : protected Implement::FilterBase<CompT...>
	{
		using Super = Implement::FilterBase<CompT...>;
		static_assert(Potato::Tmp::bool_and<true, Implement::AcceptableTypeDetector<CompT>::value...>::value, "EntityFilter only accept Type and const Type!");
		template<typename Func>
		void operator()(const Entity& wrapper, Func&& f);
	private:
		EntityFilter(Implement::ComponentPoolInterface* pool) noexcept : Implement::FilterBase<CompT...>(pool) { }
		void pre_apply() noexcept {}
		void pos_apply() noexcept {}
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
				size_t* infos = Super::find_type_layout(group);
				if (infos != nullptr)
				{
					std::tuple<std::remove_reference_t<CompT>* ...> component_pointer;
					Implement::ComponentTupleHelper<0, sizeof...(CompT)>::translate(block, infos, component_pointer);
					std::apply([&](auto ...pointer) {
						std::forward<Func>(f)(*pointer...);
					}, component_pointer);
				}
			}
		}
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