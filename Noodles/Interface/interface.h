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