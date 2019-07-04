#pragma once
#include "aid.h"
#include "..//..//Potato/tool.h"
namespace Noodles
{
	namespace Implement
	{
		struct GobalComponentInterface
		{
			virtual ~GobalComponentInterface() = default;
			virtual const TypeInfo& type_info() const noexcept = 0;
			virtual void* get_adress() noexcept = 0;
			virtual void add_ref() noexcept = 0;
			virtual void sub_ref() noexcept = 0;
		private:
			virtual void* get_adress_imp() = 0;
		};

		template<typename Type> struct GobalComponentImp : GobalComponentInterface
		{
			operator Type& () { return m_storage; }
			const TypeInfo& type_info() const noexcept { return m_info; }
			virtual void add_ref() noexcept override { m_ref.add_ref(); }
			virtual void sub_ref() noexcept override {
				if (m_ref.sub_ref())
					delete this;
			}
			virtual void* get_adress() noexcept override { return &m_storage; }
			template<typename ...Parameter>
			static Potato::Tool::intrusive_ptr<GobalComponentImp> create(Parameter&& ... para) { return new GobalComponentImp{ std::forward<Parameter>(para)... }; }
		private:
			template<typename ...Parameter> GobalComponentImp(Parameter&& ...);
			Potato::Tool::atomic_reference_count m_ref;
			const TypeInfo& m_info;
			Type m_storage;
		};

		template<typename Type> template<typename ...Parameter> GobalComponentImp<Type>::GobalComponentImp(Parameter&& ... para)
			: m_info(TypeInfo::create<Type>()), m_storage(std::forward<Parameter>(para)...)
		{}

		using GobalComponentInterfacePtr = Potato::Tool::intrusive_ptr<GobalComponentInterface>;

		struct GobalComponentPoolInterface
		{
			virtual void regedit_gobal_component(GobalComponentInterface*) noexcept = 0;
			virtual void destory_gobal_component(const TypeInfo&) = 0;
			virtual void* find(const TypeInfo& layout) const noexcept = 0;
		};

	}

	namespace Implement
	{
		template<typename Type> struct FilterAndEventAndSystem;
	}

	template<typename CompT> struct GobalFilter
	{
		static_assert(Implement::AcceptableTypeDetector<CompT>::value, "GobalFilter only accept Type and const Type!");
		operator bool() const noexcept { return m_cur != nullptr; }
		CompT* operator->() noexcept { return m_cur; }
		CompT& operator*() noexcept { return *m_cur; }

	private:

		static void export_rw_info(Implement::ReadWritePropertyMap& tuple) noexcept
		{
			Implement::TypeInfoListExtractor<CompT>{}(tuple.gobal_components);
		}

		void envirment_change(bool system, bool gobalcomponent, bool component)
		{
			if (gobalcomponent)
				m_cur = reinterpret_cast<CompT>(m_pool->find(TypeInfo::create<CompT>()));
		}
		void export_type_group_used(const TypeInfo* conflig_type, size_t conflig_count, Implement::ReadWriteProperty*) const noexcept {}
		void pre_apply() noexcept {}
		void pos_apply() noexcept {}

		GobalFilter(Implement::GobalComponentPoolInterface* in) noexcept : m_pool(in), m_cur(nullptr) { assert(m_pool != nullptr); }

		template<typename Require> friend struct Implement::FilterAndEventAndSystem;

		Implement::GobalComponentPoolInterface* m_pool = nullptr;
		CompT* m_cur;

		template<typename Require> friend struct Implement::FilterAndEventAndSystem;
	};



}