module;

#include <Windows.h>

export module NoodlesForm;

import std;
import PotatoPointer;
import PotatoTaskSystem;

export namespace Noodles::Form
{

	struct FormSetting {
		const wchar_t* Title = L"PO default title :>";
		std::uint32_t Width = 1024;
		std::uint32_t Height = 768;
		std::uint32_t ShiftX = 0;
		std::uint32_t ShiftY = 0;
	};

	struct FormStyle {

	};

	const FormStyle& DefaultStyle() noexcept;

	struct GlobalFormSetting
	{
		std::u8string display_name = u8"FormManager";
		std::size_t priority = *Potato::Task::TaskPriority::Normal;
		std::chrono::milliseconds update_rate = std::chrono::milliseconds{16};
		std::chrono::milliseconds idle_update_rate = std::chrono::milliseconds{50};
	};

	struct FormManager;

	struct FormInterface : public Potato::Pointer::DefaultStrongWeakInterface
	{
		using Ptr = Potato::Pointer::StrongPtr<FormInterface>;

		virtual bool CloseWindows() = 0;

	protected:

		using WPtr = Potato::Pointer::WeakPtr<FormInterface>;

		virtual bool Link(FormManager&) = 0;

		virtual std::optional<LRESULT> RespondEventInEventLoop(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

		friend struct FormManager;
	};


	struct FormManager : public Potato::Pointer::DefaultStrongWeakInterface
	{
		using Ptr = Potato::Pointer::StrongPtr<FormManager>;

		static Ptr Create(GlobalFormSetting const& setting = {}, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		bool CreateAndBindForm(FormInterface& form_interface);

		void CloseForm(FormInterface& form_interface);

	protected:

		using WPtr = Potato::Pointer::WeakPtr<FormManager>;

		FormManager(GlobalFormSetting const& setting, std::pmr::memory_resource*);
		void ReleaseForm(FormInterface* form);

		void Fire();

		std::pmr::memory_resource* resource;

		std::atomic_size_t exist_windows;

		std::mutex mutex;
		std::pmr::vector<FormInterface::WPtr> requests;
		GlobalFormSetting const setting;
		std::thread message_thread;

		void Execite();
		virtual void WeakRelease() override;
		virtual void StrongRelease() override;

		friend struct FormProxy;
	};

	struct FormBase : public FormInterface
	{
		
		virtual bool CloseWindows() override;

	protected:

		virtual bool Link(FormManager&) override;
		virtual void StrongRelease() override;

		std::mutex mutex;
		FormManager::Ptr linked_manager;
	};

	struct FormWindowed : public FormBase
	{
		using Ptr = Potato::Pointer::StrongPtr<FormWindowed>;

		static Ptr Create(std::pmr::memory_resource* resource = std::pmr::get_default_resource());

	protected:

		using WeakPtr = Potato::Pointer::WeakPtr<FormWindowed>;

		virtual void WeakRelease() override;
		virtual void StrongRelease() override;

		std::mutex mutex;

		~FormWindowed();

		friend struct FormManager;
	};


	/*
	struct FormProxy : public Potato::Task::ControlDefaultInterface
	{
		struct Setting
		{

		};

		using Ptr = Potato::Task::ControlPtr<FormProxy>;

		//static Create(FormProxy::Setting const& setting, )

		~FormProxy();

	protected:

		using WPtr = Potato::Pointer::IntrusivePtr<FormProxy>;

		FormProxy(FormManager* Owner, std::pmr::memory_resource* resource);

		virtual void Release() override;

		std::pmr::memory_resource* resource;
		FormManager* Owner;

		//FormProxy(Potato::Pointer::IntrusivePtr<FormManager>::Ptr manager, std::pmr::memory_resource* upstream);

		//Potato::Pointer::IntrusivePtr<FormManager> manager;

		friend struct FormManager;
	};
	*/

	

}