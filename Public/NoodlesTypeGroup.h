#pragma once
#include <typeinfo>
#include <string_view>
#include <span>
#include "Potato/Public/PotatoIR.h"

namespace Noodles::TypeGroup
{

	struct Info
	{
		Info(Info const&) = default;
		template<typename Type>
		static constexpr Info Get() { 
			using RT = std::remove_cvref_t<Type>;
			return {
				typeid(RT).hash_code(),
				Potato::IR::ClassLayout::Get<RT>(), 
				[](std::byte* Source, std::byte* Target){
					new (Source) RT { std::move(*reinterpret_cast<RT*>(Target)};
				},
				[](std::byte* Source){ reinterpret_cast<RT*>(Source)->~RT(); }
			};
		};
	private:
		Info(std::size_t HashCode, Potato::IR::ClassLayout Layout, void (*MovementFunction) (std::byte* Source, std::byte* Target), void (*Destructor)(std::byte* Source))
			: HashCode(HashCode), Layout(Layout), MovementFunction(MovementFunction), Destructor(Destructor)
		{
		}
		std::size_t HashCode;
		Potato::IR::ClassLayout Layout;
		void (*MovementFunction) (std::byte* Source, std::byte* Target);
		void (*Destructor)(std::byte* Source);
	};

	struct Group
	{
		struct Order 
		{
			std::size_t Offset;
			Info TypeInfo;
		};

		Potato::IR::ClassLayout ElementLayout;
		std::span<Order const> Order;
		std::byte* TopData;
	};

}