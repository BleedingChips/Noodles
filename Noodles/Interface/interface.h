#pragma once
#include "component_interface.h"
#include "entity_interface.h"
#include "event_interface.h"
#include "system_interface.h"

namespace Noodles
{
	struct Context
	{
		Entity create_entity() { return Entity{ create_entity_imp() }; }
		template<typename CompT, typename ...Parameter> std::remove_reference_t<std::remove_const_t<CompT>>& create_component(Entity entity, Parameter&& ...p);
		template<typename CompT, typename ...Parameter> std::remove_reference_t<std::remove_const_t<CompT>>& create_gobal_component(Parameter&& ...p);
		template<typename SystemT, typename ...Parameter> std::remove_reference_t<std::remove_const_t<SystemT>>& create_system(Parameter&& ...p);
		template<typename SystemT> void create_system(SystemT&& p, TickPriority priority = TickPriority::Normal, TickPriority layout = TickPriority::Normal);
		template<typename SystemT, typename ...Parameter> std::remove_reference_t<std::remove_const_t<SystemT>>& create_temporary_system(Parameter&& ...p);
		template<typename SystemT> void create_temporary_system(SystemT&& p, TickPriority priority = TickPriority::Normal, TickPriority layout = TickPriority::Normal);
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
		intrusive_ptr<Implement::AsynchronousWorkInterface> ptr = new Implement::AsynchronousWorkImplement<CallableObject, Parameter...>{
			std::forward<CallableObject>(co), std::forward<Parameter>(pa)...
		};
		insert_asynchronous_work_imp(ptr);
	}

	template<typename CompT> void Context::destory_gobal_component()
	{
		Implement::GobalComponentPoolInterface* GPI = *this;
		GPI->destory_gobal_component(TypeInfo::create<CompT>());
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
		return cp->deconstruct_component(entity.m_imp, TypeInfo::create<CompT>());
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
		intrusive_ptr<Implement::SystemImplement<SystemT>> ptr = new Implement::SystemImplement<SystemT>{ this, [](const Implement::SystemImplement<SystemT>* in) noexcept {delete in; }, TickPriority::Normal, TickPriority::Normal, std::forward<Parameter>(p)... };
		SI->regedit_system(ptr);
		return *ptr;
	}

	template<typename SystemT> void Context::create_system(SystemT&& p, TickPriority priority, TickPriority layout)
	{
		Implement::SystemPoolInterface* SI = *this;
		intrusive_ptr<Implement::SystemImplement<SystemT>> ptr = new Implement::SystemImplement<SystemT>{ this, [](const Implement::SystemImplement<SystemT>* in) noexcept {delete in; }, priority, layout, std::forward<SystemT>(p) };
		SI->regedit_system(ptr);
	}

	template<typename SystemT, typename ...Parameter> std::remove_reference_t<std::remove_const_t<SystemT>>& Context::create_temporary_system(Parameter&& ...p)
	{
		Implement::SystemPoolInterface* SI = *this;
		intrusive_ptr<Implement::SystemImplement<SystemT>> ptr = new Implement::SystemImplement<SystemT>{ this, [](const Implement::SystemImplement<SystemT>* in) noexcept {delete in; }, TickPriority::Normal, TickPriority::Normal, std::forward<Parameter>(p)... };
		SI->regedit_template_system(ptr);
		return *ptr;
	}

	template<typename SystemT> void Context::create_temporary_system(SystemT&& p, TickPriority priority, TickPriority layout)
	{
		Implement::SystemPoolInterface* SI = *this;
		intrusive_ptr<Implement::SystemImplement<SystemT>> ptr = new Implement::SystemImplement<SystemT>{ this, [](const Implement::SystemImplement<SystemT>* in) noexcept {delete in; }, priority, layout, std::forward<SystemT>(p) };
		SI->regedit_template_system(ptr);
	}

	template<typename SystemT> void Context::destory_system()
	{
		Implement::SystemPoolInterface* SI = *this;
		SI->destory_system(TypeInfo::create<SystemT>());
	}
}


/*
namespace Noodles
{
	using namespace Potato;

	struct Context;

	namespace Implement
	{

		template<typename ...Require> struct SystemStorage;
		template<typename Type> struct FilterAndEventAndSystem;
		struct ComponentPoolInterface;
	}

	namespace Implement
	{

		

		

		using EntityInterfacePtr = Tool::intrusive_ptr<EntityInterface>;
	}

	

}

namespace Noodles
{
	namespace Implement
	{
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
	}

	

	template<typename ...CompT> struct FilterIterator;

	namespace Implement
	{
		
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
*/