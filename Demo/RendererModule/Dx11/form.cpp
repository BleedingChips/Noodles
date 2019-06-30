#include "form.h"
#include <vector>

constexpr auto UD_REQUEST_QUIT = (WM_USER + 1);

namespace
{
	LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	const char16_t static_class_name[] = u"po_frame_default_win32_class";
	const WNDCLASSEXW static_class = { sizeof(WNDCLASSEXW), CS_HREDRAW | CS_VREDRAW , WndProc, 0, 0, GetModuleHandle(0), NULL,NULL, 0, NULL, (const wchar_t*)static_class_name, NULL };
	const struct StaticClassInitStruct
	{
		StaticClassInitStruct() { HRESULT res = RegisterClassExW(&static_class); assert(SUCCEEDED(res)); }
		~StaticClassInitStruct() { UnregisterClassW((const wchar_t*)static_class_name, GetModuleHandleW(0)); }
	}init;

	std::tuple<DWORD, DWORD> translate_style(Win32::Style style)
	{
		switch (style)
		{
		case Win32::Style::Normal:
			return { 0, WS_OVERLAPPEDWINDOW };
		default:
			return { 0, 0 };
		}
	}

	LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (msg)
		{
		case UD_REQUEST_QUIT:
		{
			DestroyWindow(hWnd);
			LONG_PTR data = GetWindowLongPtr(hWnd, GWLP_USERDATA);
			Win32::Implement::Control* ptr = reinterpret_cast<Win32::Implement::Control*>(data);
			delete ptr;
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(nullptr));
			return 0;
		}
		default:
		{
			LONG_PTR data = GetWindowLongPtr(hWnd, GWLP_USERDATA);
			Win32::Implement::Control* ptr = reinterpret_cast<Win32::Implement::Control*>(data);
			if (ptr != nullptr)
			{
				std::lock_guard lg(ptr->m_mutex);
				ptr->m_msages.push_back(MSG{ hWnd, msg, wParam, lParam });
			}
			if (msg == WM_CLOSE)
				return 0;
			return DefWindowProcW(hWnd, msg, wParam, lParam);
		}
		}
	}

	struct GobalManager
	{
		Potato::Tool::intrusive_ptr<Win32::Implement::Control> create_control(const Win32::FormProperty& pro);
		~GobalManager();
	private:
		static void execute_function(GobalManager* ins);
		std::mutex state_lock;
		bool read_to_exit = true;
		size_t count = 0;
		std::vector<std::tuple<std::promise<void>&, const Win32::FormProperty&, Win32::Implement::Control*>> requests;
		std::thread execute_thread;
	};

	GobalManager::~GobalManager()
	{
		if (execute_thread.joinable())
			execute_thread.join();
	}

	void GobalManager::execute_function(GobalManager* ins)
	{
		bool available = true;
		while (available)
		{
			{
				std::lock_guard lg(ins->state_lock);
				if (!ins->requests.empty())
				{
					for (auto& ite : ins->requests)
					{
						auto& setting = std::get<1>(ite);
						auto type = translate_style(setting.style);
						HWND handle = CreateWindowExW(
							std::get<0>(type),
							(wchar_t*)(static_class_name),
							(wchar_t*)(setting.title.c_str()),
							WS_VISIBLE | std::get<1>(type),
							setting.shift_x, setting.shift_y, setting.width, setting.height,
							NULL,
							NULL,
							GetModuleHandle(0),
							NULL
						);
						if (handle != nullptr)
						{
							SetWindowLongPtr(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(std::get<2>(ite)));
							std::get<2>(ite)->m_handle = handle;
							std::get<0>(ite).set_value();
							++ins->count;
						}
						else 
							std::get<0>(ite).set_exception(std::make_exception_ptr(Win32::Error::CreateWindowFauit{ GetLastError() }));
					}
					ins->requests.clear();
				}
				else if (ins->count == 0)
				{
					ins->read_to_exit = true;
					available = false;
				}
			}
			MSG msg;
			while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
			{
				if (msg.message == UD_REQUEST_QUIT)
				{
					std::lock_guard lg(ins->state_lock);
					--ins->count;
				}
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
		}
	}

	Potato::Tool::intrusive_ptr<Win32::Implement::Control> GobalManager::create_control(const Win32::FormProperty& pro)
	{
		std::promise<void> promise;
		auto future = promise.get_future();
		Potato::Tool::intrusive_ptr<Win32::Implement::Control> control = new Win32::Implement::Control{};
		{
			std::lock_guard lg(state_lock);
			if (read_to_exit)
			{
				if (execute_thread.joinable())
					execute_thread.join();
				execute_thread = std::thread(execute_function, this);
			}
			requests.push_back(std::forward_as_tuple(promise, pro, control));
		}
		future.get();
		return std::move(control);
	}

	GobalManager gobal;
}



namespace Win32
{
	namespace Error
	{
		const char* CreateWindowFauit::what() const noexcept { return "unable to create windows"; }
	}

	namespace Implement
	{

		Control::Control(): m_handle(nullptr) {}
		void Control::add_ref() noexcept { m_ref.add_ref(); }
		void Control::sub_ref() noexcept
		{
			if (m_ref.sub_ref())
				PostMessageW(m_handle, UD_REQUEST_QUIT, 0, 0);
		}
	}

	bool Form::pook_event(MSG& out) noexcept
	{
		assert(m_ref);
		std::lock_guard lg(m_ref->m_mutex);
		if (!m_ref->m_msages.empty())
		{
			out = *m_ref->m_msages.begin();
			m_ref->m_msages.pop_front();
			return true;
		}
		return false;
	}

	Form Form::create(const FormProperty & pro)
	{
		Form result;
		result.m_ref = gobal.create_control(pro);
		return std::move(result);
	}
}