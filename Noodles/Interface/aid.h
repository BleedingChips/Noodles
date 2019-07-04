#pragma once
#include <map>
#include "..//..//Potato/smart_pointer.h"
namespace Noodles
{

	using Potato::Tool::intrusive_ptr;
	using Potato::Tool::observer_ptr;

	struct TypeInfo
	{
		size_t hash_code;
		size_t size;
		size_t align;
		const char* name;
		~TypeInfo() = default;
		template<typename Type> static const TypeInfo& create() noexcept {
			static TypeInfo type{ typeid(Type).hash_code(), sizeof(Type), alignof(Type), typeid(Type).name() };
			return type;
		}
		bool operator<(const TypeInfo& r) const noexcept
		{
			if (hash_code < r.hash_code)
				return true;
			else if (hash_code == r.hash_code)
			{
				if (size < r.size)
					return true;
				else if (size == r.size)
				{
					if (align < r.align)
						return true;
				}
			}
			return false;
		}
		bool operator<=(const TypeInfo& r) const noexcept
		{
			return (*this) < r || (*this) == r;
		}
		bool operator==(const TypeInfo& type) const noexcept
		{
			return hash_code == type.hash_code && size == type.size && align == type.align;
		}
		bool operator!=(const TypeInfo& type) const noexcept
		{
			return !(*this == type);
		}
	};

	namespace Implement
	{
		enum class ReadWriteProperty : uint8_t
		{
			Read = 0,
			Write = 1,
			Unknow = 2
		};

		struct ReadWritePropertyMap
		{
			std::map<TypeInfo, ReadWriteProperty> gobal_components;
			std::map<TypeInfo, ReadWriteProperty> components;
			std::map<TypeInfo, ReadWriteProperty> systems;
		};

		template<typename ...AT> struct TypeInfoListExtractor
		{
			void operator()(std::map<TypeInfo, ReadWriteProperty>& result) {}
			static constexpr ReadWriteProperty read_write_property = ReadWriteProperty::Read;
		};

		struct TypeGroup;

		struct EntityInterface;

		struct StorageBlockFunctionPair
		{
			void (*destructor)(void*) noexcept = nullptr;
			void (*mover)(void*, void*) noexcept = nullptr;
		};

		struct StorageBlock
		{
			const TypeGroup* m_owner = nullptr;
			StorageBlock* front = nullptr;
			StorageBlock* next = nullptr;
			size_t available_count = 0;
			StorageBlockFunctionPair** functions = nullptr;
			void** datas = nullptr;
			EntityInterface** entitys = nullptr;
		};

		template<typename T> struct AcceptableTypeDetector {
			using pure_type = std::remove_reference_t<std::remove_cv_t<T>>;
			static constexpr bool is_const = std::is_same_v<T, std::add_const_t<pure_type>>;
			static constexpr bool is_pure = std::is_same_v<T, pure_type>;
			static constexpr bool value = is_const || is_pure;
		};

		template<typename ...CompT> struct TypeInfoList
		{
			static const std::array<TypeInfo, sizeof...(CompT)>& info() noexcept { return m_info; }
		private:
			static const std::array<TypeInfo, sizeof...(CompT)> m_info;
		};

		template<typename ...CompT> const std::array<TypeInfo, sizeof...(CompT)> TypeInfoList<CompT...>::m_info = {
			TypeInfo::create<CompT>()...
		};

		template<typename Require> struct FilterAndEventAndSystem;

	}
}