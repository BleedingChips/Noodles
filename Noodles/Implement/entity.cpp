#include "entity.h"
#include "component_pool.h"
namespace Noodles::Implement
{

	void EntityImp::read(TypeGroup*& a, StorageBlock*& b, size_t& index) const noexcept
	{
		a = m_type_group; b = m_storage_block; index = m_index;
	}

	void EntityImp::set(TypeGroup* a, StorageBlock* b, size_t index) noexcept
	{
		m_type_group = a;
		m_storage_block = b;
		m_index = index;
	}

	bool EntityImp::have(const TypeInfo* ty, size_t index) const noexcept
	{
		if (index == 0)
			return true;
		else if (m_type_group != nullptr)
		{
			return m_type_group->layouts().hold_unordered(ty, index);
		}
		else
			return false;
	}

	void EntityImp::add_ref() const noexcept
	{
		m_ref.add_ref();
	}

	void EntityImp::sub_ref() const noexcept
	{
		if (m_ref.sub_ref())
			delete this;
	}

	intrusive_ptr<EntityImp> EntityImp::create_one()
	{
		return new EntityImp{};
	}

	EntityImp::~EntityImp()
	{
		assert(m_type_group == nullptr);
	}
}