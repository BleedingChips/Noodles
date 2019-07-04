#pragma once
#include "../Interface/gobal_component_interface.h"
#include <shared_mutex>
#include <map>
#include <variant>
namespace Noodles::Implement
{
	struct GobalComponentPool : GobalComponentPoolInterface
	{
		virtual void* find(const TypeInfo& layout) const noexcept override;
		virtual void regedit_gobal_component(GobalComponentInterface*) noexcept override;
		virtual void destory_gobal_component(const TypeInfo& layout) noexcept override;
		void update();
		void clean_all();
		mutable std::shared_mutex m_list_mutex;
		std::map<TypeInfo, GobalComponentInterfacePtr> m_lists;
		std::mutex m_write_list_mutex;
		std::vector<std::variant<GobalComponentInterfacePtr, TypeInfo>> m_list;
	};


}