#pragma once
#include <typeinfo>
#include <string_view>
#include <span>
#include "Potato/Public/PotatoMisc.h"

namespace Noodles
{
	struct TypeInfo
	{
		std::size_t HashCode;
		std::string_view DebugName;
		template<typename Type>
		constexpr static const TypeInfo& Create() noexcept {
			static TypeInfo type{ typeid(Type).hash_code(), typeid(Type).name() };
			return type;
		}
		constexpr std::strong_ordering operator<=>(TypeInfo const&) const = default;
	};

	struct TypeDesc
	{
		TypeInfo Info;

	};

	struct TypeGroup
	{
		std::span<TypeInfo> GroupInfos;
		std::byte* MemoryPageStart;
		//std::byte* 
	};

}