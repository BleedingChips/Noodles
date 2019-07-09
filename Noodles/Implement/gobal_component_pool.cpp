#include "gobal_component_pool.h"
namespace Noodles::Implement
{

	void* GobalComponentPool::find(const TypeInfo& layout) const noexcept
	{
		auto ite = m_lists.find(layout);
		if (ite != m_lists.end())
			return ite->second->get_adress();
		else
			return nullptr;
	}

	void GobalComponentPool::regedit_gobal_component(GobalComponentInterface* in) noexcept
	{
		assert(in != nullptr);
		std::lock_guard lg(m_write_list_mutex);
		m_list.push_back(in);
	}

	void GobalComponentPool::destory_gobal_component(const TypeInfo& layout) noexcept
	{
		std::lock_guard lg(m_write_list_mutex);
		m_list.push_back(layout);
	}

	void GobalComponentPool::clean_all()
	{
		std::unique_lock ul(m_read_mutex);
		std::lock_guard lg(m_write_list_mutex);
		m_lists.clear();
		m_list.clear();
	}

	bool GobalComponentPool::update()
	{
		std::unique_lock ul(m_read_mutex);
		std::lock_guard lg(m_write_list_mutex);
		bool gobal_component_update = false;
		if (!m_list.empty())
		{
			gobal_component_update = true;
			for (auto& ite : m_list)
			{
				std::visit(Potato::Tool::overloaded{
					[&](GobalComponentInterfacePtr& ptr) {
					assert(ptr);
					auto& layout = ptr->type_info();
					m_lists[layout] = std::move(ptr);
				},
					[&](const TypeInfo& layout) {
					auto ite = m_lists.find(layout);
					m_lists.erase(ite);
				} }, ite);
			}
			m_list.clear();
		}
		return gobal_component_update;
	}
}