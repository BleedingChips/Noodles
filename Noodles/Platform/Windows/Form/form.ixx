module;

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

	struct Form : public Potato::Pointer::DefaultIntrusiveInterface
	{

		struct Setting
		{
			
		};

		using Ptr = Potato::Pointer::IntrusivePtr<Form>;

		static Ptr Create(Potato::Task::ControlPtr<FormManager> manager, Setting const& setting, );

	protected:
		std::pmr::memory_resource* resource;
		virtual void Release() override;

		friend struct FormManager;
	};



	struct FormManager : protected Potato::Task::Task
	{
		using Ptr = Potato::Task::ControlPtr<FormManager>;

		static Ptr Create(Potato::Task::TaskContext::Ptr context, GlobalFormSetting const& setting = {}, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

	protected:

		FormManager(Potato::Task::TaskContext::Ptr context, GlobalFormSetting const& setting, std::pmr::memory_resource*);
		void Fire();

		std::pmr::memory_resource* resource;
		GlobalFormSetting setting;
		Potato::Task::TaskContext::Ptr context;
		std::atomic_bool available = true;

		virtual void operator()(Potato::Task::ExecuteStatus& status) override;
		virtual void Release() override;
		virtual void ControlRelease() override;
	};


	struct Form : public Potato::Pointer::DefaultIntrusiveInterface
	{
		
	};


}