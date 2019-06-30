#include "../include/gobal_component_pool.h"
namespace Noodles::Implement
{
	GobalComponentInterface* GobalComponentPool::find_imp(const TypeLayout& layout) const noexcept
	{
		std::shared_lock sl(m_list_mutex);
		auto ite = m_lists.find(layout);
		if (ite != m_lists.end())
			return ite->second;
		else 
			return nullptr;
	}

	void GobalComponentPool::regedit_gobal_component(GobalComponentInterface* in) noexcept
	{
		assert(in != nullptr);
		std::lock_guard lg(m_write_list_mutex);
		m_list.push_back(in);
	}

	void GobalComponentPool::destory_gobal_component(const TypeLayout& layout) noexcept
	{
		std::lock_guard lg(m_write_list_mutex);
		m_list.push_back(layout);
	}

	void GobalComponentPool::clean_all()
	{
		std::unique_lock ul(m_list_mutex);
		std::lock_guard lg(m_write_list_mutex);
		m_lists.clear();
		m_list.clear();
	}

	void GobalComponentPool::update()
	{
		std::unique_lock ul(m_list_mutex);
		std::lock_guard lg(m_write_list_mutex);
		for (auto& ite : m_list)
		{
			std::visit(Tool::overloaded{
				[&](GobalComponentInterfacePtr& ptr) {
				assert(ptr);
				auto& layout = ptr->layout();
				m_lists[layout] = std::move(ptr);
			},
				[&](const TypeLayout& layout) {
				auto ite = m_lists.find(layout);
				m_lists.erase(ite);
			}}, ite);
		}
		m_list.clear();
	}
}