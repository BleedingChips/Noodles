#pragma once
#include <atomic>
#include <array>
namespace Noodle
{
	struct MemoryPageDescription
	{
		MemoryPageDescription* FrontPage;
		MemoryPageDescription* LastPage;
		std::byte* StartEntityPtr;
		std::byte* EndEntityPtr;
	};
}