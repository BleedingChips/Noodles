#pragma once
#include "aid.h"
#include "..//..//Potato/intrusive_ptr.h"
#include "component_interface.h"
#include "event_interface.h"
#include "gobal_component_interface.h"

namespace Noodles
{
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

	struct Context;

	namespace Implement
	{
		struct SystemInterface
		{
			virtual void* data() noexcept = 0;
			virtual const TypeInfo& layout() const noexcept = 0;
			virtual void apply(Context*) noexcept = 0;
			virtual void add_ref() noexcept = 0;
			virtual void sub_ref() noexcept = 0;
			virtual TickPriority tick_layout() = 0;
			virtual TickPriority tick_priority() = 0;
			virtual TickOrder tick_order(const TypeInfo&) = 0;
			virtual void out_read_write_property(const TypeInfo*& storage, const ReadWriteProperty*& property, const size_t*& count) const noexcept = 0;
			virtual void out_type_group_usage(size_t*& type_group_useage, size_t& usage_count) const noexcept = 0;
		};

		using SystemInterfacePtr = Potato::Tool::intrusive_ptr<SystemInterface>;

		struct SystemPoolInterface
		{
			virtual void* find_system(const TypeInfo& ti) noexcept = 0;
			virtual void regedit_system(SystemInterface*) noexcept = 0;
			virtual void destory_system(const TypeInfo& id) noexcept = 0;
			virtual void regedit_template_system(SystemInterface*) noexcept = 0;
		};


	}

	template<typename Type>
	struct SystemFilter
	{
		static_assert(Implement::AcceptableTypeDetector<Type>::value, "SystemWrapper only accept Type and const Type!");
		operator bool() const noexcept { return m_resource != nullptr; }
		Type* operator->() noexcept { return m_resource; }
		Type& operator*() noexcept { return *m_resource; }

	private:

		SystemFilter(Implement::SystemPoolInterface* pool) noexcept : m_pool(pool), m_resource(nullptr) {}

		void type_group_change() noexcept {}
		void system_change() noexcept { m_cur = reinterpret_cast<Type*>(m_pool->find_system(TypeInfo::create<Type>())); };
		void gobal_component_change() noexcept {  }
		void pre_apply() noexcept {}
		void pos_apply() noexcept {}
		static void export_rw_info(Implement::ReadWritePropertyMap& tuple) noexcept { Implement::TypeInfoListExtractor<Type>{}(tuple.systems); }
		void export_type_group_used(Implement::ReadWriteProperty* RWP) const noexcept {}

		Type* m_resource;
		Implement::SystemPoolInterface* m_pool;
		template<typename Require> friend struct Implement::FilterAndEventAndSystem;
	};

	namespace Implement
	{
		template<typename Type> struct ContextStorage
		{
			void type_group_change() noexcept {}
			void system_change() noexcept { };
			void gobal_component_change() noexcept {  }
			void pre_apply() noexcept {}
			void pos_apply() noexcept {}
			static void export_rw_info(Implement::ReadWritePropertyMap& tuple) noexcept { }
			void export_type_group_used(Implement::ReadWriteProperty* RWP) const noexcept {}
			Context* as_pointer() noexcept { return m_ref; }
			ContextStorage(Context* input) noexcept : m_ref(input) {}
		private:
			using PureType = std::remove_const_t<Type>;
			static_assert(std::is_same_v<PureType, Context&>, "System require Parameter Should be \"Context&\" but not \"Context\"");
			Context* m_ref = nullptr;
		};

		template<typename Type> struct FilterAndEventAndSystem
		{
			void pre_apply() noexcept { m_storage.pre_apply(); }
			void pos_apply() noexcept { m_storage.pos_apply(); }
			void type_group_change() noexcept { m_storage.type_group_change(); }
			void system_change() noexcept { m_storage.system_change(); };
			void gobal_component_change() noexcept { m_storage.gobal_component_change(); }
			
			static void export_rw_info(Implement::ReadWritePropertyMap& tuple) noexcept { PureType::export_rw_info(tuple); }
			void export_type_group_used(Implement::ReadWriteProperty* RWP) const noexcept { m_storage.export_type_group_used(RWP); }

			std::remove_reference_t<Type>* as_pointer() noexcept { return &m_storage; }
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
	}

}