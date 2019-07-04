#pragma once
#include "aid.h"
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
		struct SystemInterface;

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

		void envirment_change(bool system, bool gobalcomponent, bool component)
		{
			if (system)
				m_resource = reinterpret_cast<Type*>(m_pool->find_system(TypeInfo::create<Type>()));
		}
		void export_type_group_used(const TypeInfo* conflig_type, size_t conflig_count, Implement::ReadWriteProperty*) const noexcept {}
		void pre_apply() noexcept {}
		void pos_apply() noexcept {}
		static void export_rw_info(Implement::ReadWritePropertyMap& tuple) noexcept { Implement::TypeInfoListExtractor<Type>{}(tuple.systems); }

		observer_ptr<Type> m_resource;
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
			void envirment_change(bool system, bool gobalcomponent, bool component) { m_storage.envirment_change(system, gobalcomponent, component); }
			static void export_rw_info(Implement::ReadWritePropertyMap& tuple) noexcept { PureType::export_rw_info(tuple); }
			void export_type_group_used(const TypeInfo* conflig_type, size_t conflig_count, Implement::ReadWriteProperty* RW) const noexcept { 
				m_storage.export_type_group_used(conflig_type, conflig_count, RW);
			}

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

		template<typename ...Require>
		struct SystemRWInfo
		{
			SystemRWInfo();
			void rw_property(const TypeInfo*& layout, const ReadWriteProperty*& property, const size_t*& index) const noexcept
			{
				layout = m_rw_type.data(); property = m_rw_property.data(); index = m_index.data();
			}
		private:
			std::vector<TypeInfo> m_rw_type;
			std::vector<ReadWriteProperty> m_rw_property;
			std::array<size_t, 3> m_index;
		};

		template<typename ...Require> SystemRWInfo<Require...>::SystemRWInfo()
		{
			Implement::ReadWritePropertyMap tuple;
			Potato::Tool::sequence_call([&](auto& ref) {
				std::remove_reference_t<decltype(ref)>::export_rw_info(tuple);
			}, *reinterpret_cast<std::tuple<Require...>*>(nullptr));
			m_index[0] = tuple.systems.size();
			m_index[1] = tuple.gobal_components.size();
			m_index[2] = tuple.components.size();
			size_t total_size = m_index[0] + m_index[1] + m_index[2];
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
				Potato::Tool::sequence_call([&](auto& a) {a.pre_apply(); }, m_storage);
				std::apply([&](auto&& ... ai) { sys(*ai.as_pointer()...); }, m_storage);
				Potato::Tool::sequence_call([&](auto& a) {a.pos_apply(); }, m_storage);
			}
			static SystemRWInfo<Requires...> rw_info;
			SystemStorage(Context* in) : m_storage(CopyContext<sizeof...(Requires)>{}(in)) {}
		private:
			std::tuple<typename SystemStorageDetector<Requires>::Type...> m_storage;
		};

	}

	namespace Implement
	{
		template<typename Type, typename = decltype(&Type::tick_layout)> struct TickLayoutDetector {};
		template<typename Type, typename = decltype(&Type::tick_priority)> struct TickPriorityDetector {};
		template<typename Type, typename = decltype(&Type::tick_order)> struct TickOrderDetector {};

		struct SystemInterface
		{
			virtual void* data() noexcept = 0;
			virtual const TypeInfo& layout() const noexcept = 0;
			virtual void apply(Context*) noexcept = 0;
			virtual void add_ref() noexcept = 0;
			virtual void sub_ref() noexcept = 0;
			virtual TickPriority tick_layout() = 0;
			virtual TickPriority tick_priority() = 0;
			virtual TickOrder tick_order(const TypeInfo&, const TypeInfo* conflig, size_t* conflig_size) = 0;
			virtual void rw_property(const TypeInfo*& storage, const ReadWriteProperty*& property, const size_t*& count) const noexcept = 0;
			virtual void type_group_usage(const TypeInfo* infos, size_t info_count, ReadWriteProperty* type_group_useage) const noexcept = 0;
		};

		using SystemInterfacePtr = Potato::Tool::intrusive_ptr<SystemInterface>;

		template<typename Type> struct SystemImplement : SystemInterface
		{
			virtual void* data() noexcept { return &m_storage; }
			using PureType = std::remove_const_t<std::remove_reference_t<Type>>;
			using ParameterType = Potato::Tmp::function_type_extractor<PureType>;
			using AppendType = typename ParameterType::template extract_parameter<SystemStorage>;
			virtual void apply(Context* con) noexcept override { m_append_storgae.apply(m_storage, con); }
			virtual void add_ref() noexcept { m_ref.add_ref(); };
			virtual void sub_ref() noexcept { if (m_ref.sub_ref()) m_deconstructor(this); }
			virtual const TypeInfo& layout() const noexcept override { return TypeInfo::create<Type>(); }
			template<typename ...Parameter> SystemImplement(
				Context* in, void (*deconstructor)(const SystemImplement<Type>*) noexcept,
				TickPriority prioerity, TickPriority layout,
				Parameter&& ... para
			);
			virtual TickPriority tick_layout() override {
				if constexpr (Potato::Tmp::member_exist<TickLayoutDetector, Type>::value)
					return (m_outside_layout == TickPriority::Normal) ? m_storage.tick_layout() : m_outside_layout;
				else
					return m_outside_layout;
			}
			virtual TickPriority tick_priority() override {
				if constexpr (Potato::Tmp::member_exist<TickPriorityDetector, Type>::value)
					return (m_outside_priority == TickPriority::Normal) ? m_storage.tick_priority() : m_outside_priority;
				else
					return m_outside_priority;
			}

			virtual TickOrder tick_order(const TypeInfo&, const TypeInfo* conflig, size_t* conflig_size) override {
				if constexpr (Potato::Tmp::member_exist<TickOrderDetector, Type>::value)
					return m_storage.tick_order(layout, conflig, conflig_size);
				else
					return TickOrder::Undefine;
			};
			virtual void rw_property(const TypeInfo*& storage, const ReadWriteProperty*& property, const size_t*& count) const noexcept override {
				AppendType::rw_info.rw_property(storage, property, count);
			}
			virtual void type_group_usage(const TypeInfo* infos, size_t info_count, ReadWriteProperty* type_group_useage) const noexcept override {
				m_append_storgae.export_type_group_usage(infos, info_count, type_group_useage);
			}
			operator std::remove_reference_t<Type>& () noexcept { return m_storage; }
		private:
			//virtual std::type_index id() const noexcept { return typeid(Type); }
			Potato::Tool::atomic_reference_count m_ref;
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
			Potato::Tool::atomic_reference_count m_ref;

			AsynchronousWorkImplement(CallableObject&& object, Parameter&& ... pa)
				: object(std::forward<CallableObject>(object)), parameter(std::forward<Parameter>(pa)...) {}

		};
	}
}