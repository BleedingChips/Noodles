#pragma once
#include <future>
#include <string>
#include <Windows.h>
#include <deque>
#include "..//..//..//Potato/smart_pointer.h"
#include "..//..//..//Potato/tool.h"
namespace Win32
{

	enum class Style
	{
		Normal,
	};

	namespace Error {
		struct CreateWindowFauit : std::exception
		{
			DWORD m_result;
			const char* what() const noexcept override;
			CreateWindowFauit(DWORD result) : m_result(result) {}
		};
	}

	struct FormProperty
	{
		Style style = Style::Normal;
		std::u16string title = u"PO default title :>";
		int shift_x = (GetSystemMetrics(SM_CXSCREEN) - 1024) / 2;
		int shift_y = (GetSystemMetrics(SM_CYSCREEN) - 768) / 2;
		int width = 1024;
		int height = 768;
	};

	

	struct Form;
	

	namespace Implement
	{
		struct Control
		{
			Control();
			void add_ref() noexcept;
			void sub_ref() noexcept;
			HWND m_handle;
			Potato::Tool::atomic_reference_count m_ref;
			mutable std::mutex m_mutex;
			std::deque<MSG> m_msages;
		};
	}

	struct Form
	{
		operator bool() const noexcept { return m_ref; }
		operator HWND() const noexcept { return m_ref->m_handle; }
		bool pook_event(MSG&) noexcept;
		static Form create(const FormProperty & = FormProperty{});
		Form() = default;
		Form(Form&&) = default;
		Form& operator=(Form&&) = default;
	private:
		Potato::Tool::intrusive_ptr<Implement::Control> m_ref;
		friend struct FormContext;
	};

}