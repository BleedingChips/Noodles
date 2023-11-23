module;

#include <cassert>
#include <Windows.h>

module NoodlesForm;

namespace Noodles::Form
{

	std::optional<LRESULT> FormInterface::RespondEventInEventLoop(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		return {};
	}

	FormManager::FormManager(GlobalFormSetting const& setting, std::pmr::memory_resource* resource)
		: setting(setting), resource(resource)
	{
		
	}

	auto FormManager::Create(GlobalFormSetting const& setting, std::pmr::memory_resource* resource)
	->Ptr
	{
		if(resource != nullptr)
		{
			auto ad = resource->allocate(sizeof(FormManager), alignof(FormManager));
			if(ad != nullptr)
			{
				Ptr ptr{new (ad) FormManager {setting, resource}};
				return ptr;
			}
		}
		return {};
	}

	bool FormManager::CreateAndBindForm(FormInterface& form_interface)
	{
		if(form_interface.Link(*this))
		{
			std::lock_guard lg(mutex);
			FormInterface::Ptr ptr{ &form_interface };
			requests.emplace_back(ptr);
			std::size_t E = 0;
			while(!exist_windows.compare_exchange_strong(E, E + 1)) {}
			if(E == 0)
			{
				message_thread.join();
				Ptr man (this);
				WPtr wp{man};
				message_thread = std::thread([wp = std::move(wp), this]()
				{
					Execite();
				});
			}
			return true;
		}
		return false;
	}

	void FormManager::Execite()
	{
		
	}
	
	bool FormBase::Link(FormManager& manager)
	{
		std::lock_guard lg(mutex);
		if(linked_manager)
		{
			return false;
		}else
		{
			linked_manager = &manager;
			return true;
		}
	}

	bool FormBase::CloseWindows()
	{
		std::lock_guard lg(mutex);
		if(linked_manager)
		{
			linked_manager->CloseForm(*this);
			return true;
		}
		return false;
	}

	void FormBase::StrongRelease()
	{
		CloseWindows();
	}

	/*
	FormInterface::FormInterface(FormManager* Owner, std::pmr::memory_resource* resource)
		: resource(resource), Owner(Owner)
	{
		
	}

	FormProxy::~FormProxy()
	{
		if(Owner != nullptr)
		{
			Owner->ReleaseForm(this);
		}
	}

	void FormProxy::Release()
	{
		auto old = resource;
		this->~FormProxy();
		old->deallocate(this, sizeof(FormProxy), alignof(FormProxy));
	}

	FormManager::FormManager(GlobalFormSetting const& setting, std::pmr::memory_resource* resource)
		: setting(setting), resource(resource), requests(resource)
	{
		
	}

	FormProxy::Ptr FormManager::CreateForm(FormSetting const& setting, std::pmr::memory_resource* resource)
	{
		if(resource != nullptr)
		{
			auto form = FormProxy
			std::lock_guard lg(mutex);
			
			requests
		}
	}

	auto FormManager::Create(GlobalFormSetting const& setting, std::pmr::memory_resource* resource)
		-> Ptr
	{
		if(resource != nullptr)
		{
			auto adre = resource->allocate(sizeof(FormManager), alignof(FormManager));
			if(adre != nullptr)
			{
				Ptr ptr = new (adre) FormManager{
					setting, resource
				};
				assert(ptr);
				ptr->Fire();
				return ptr;
			}
		}
		return {};
	}


	void FormManager::Release()
	{
		auto ores = resource;
		this->~FormManager();
		ores->deallocate(this, sizeof(FormManager), alignof(FormManager));
	}


	void FormManager::operator()(Potato::Task::ExecuteStatus& status)
	{
		if(available)
		{
			bool has_message = false;
			MSG msg;
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				has_message = true;
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
			if(has_message)
			{
				context->CommitDelayTask(this, setting.update_rate, status.Property);
			}else
			{
				context->CommitDelayTask(this, setting.idle_update_rate, status.Property);
			}
		}
	}


	void FormManager::ControlRelease()
	{
		//available = false;
	}
	*/
}