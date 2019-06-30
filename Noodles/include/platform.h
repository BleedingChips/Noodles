#pragma once
#ifdef _WIN32
#include "windows.h"
#else
#endif //_Win32

namespace Noodles
{
#if _WIN32
	using process_id = HINSTANCE;
	struct process_scription
	{
		static process_scription& instance();
		process_id id = nullptr;
	private:
		process_scription() {}
	};


	struct cpu_info
	{
		SYSTEM_INFO  info;
	};
	
	

	struct platform_info
	{
		SYSTEM_INFO  info;
		static const platform_info& instance();
	public:
		platform_info();
		platform_info(const platform_info&) = delete;
		size_t cpu_count() const noexcept { return static_cast<size_t>(info.dwNumberOfProcessors); }
	};

#endif // _WIN32
}