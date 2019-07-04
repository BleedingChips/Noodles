#pragma once
#include "aid.h"
#include <assert.h>
namespace Noodles
{
	namespace Implement
	{
		struct EntityInterface
		{
			virtual void add_ref() const noexcept = 0;
			virtual void sub_ref() const noexcept = 0;
			virtual void read(TypeGroup*&, StorageBlock*&, size_t& index) const noexcept = 0;
			virtual void set(TypeGroup*, StorageBlock*, size_t index) noexcept = 0;
			virtual bool have(const TypeInfo*, size_t index) const noexcept = 0;
		};

		using EntityInterfacePtr = intrusive_ptr<EntityInterface>;
	}

	struct Context;
	template<typename ...CompT> struct EntityFilter;

	struct Entity
	{
		operator bool() const noexcept { return m_imp; }
		template<typename ...Type> bool have() const noexcept
		{
			assert(m_imp);
			std::array<TypeInfo, sizeof...(Type)> infos = { TypeInfo::create<Type>()... };
			return m_imp->have(infos.data(), infos.size());
		}
		Entity(const Entity&) = default;
		Entity(Entity&&) = default;
		Entity() = default;
		Entity& operator=(const Entity&) = default;
		Entity& operator=(Entity&&) = default;
		Entity(Implement::EntityInterfacePtr ptr) : m_imp(std::move(ptr)) {}
	private:
		Implement::EntityInterfacePtr m_imp;

		friend struct EntityWrapper;
		template<typename ...CompT> friend struct EntityFilter;
		friend struct Context;
	};

}