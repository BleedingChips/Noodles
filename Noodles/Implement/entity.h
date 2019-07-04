#pragma once
#include <shared_mutex>
#include "..//Interface/interface.h"

namespace Noodles::Implement
{
	struct ComponentMemoryPageDesc;

	struct EntityImp : EntityInterface
	{
		virtual void add_ref() const noexcept override;
		virtual void sub_ref() const noexcept override;

		virtual void read(TypeGroup*&, StorageBlock*&, size_t& index) const noexcept override;
		virtual void set(TypeGroup*, StorageBlock*, size_t index) noexcept override;
		virtual bool have(const TypeInfo*, size_t index) const noexcept override;

		static intrusive_ptr<EntityImp> create_one();

		~EntityImp();
	private:

		EntityImp() = default;

		mutable std::shared_mutex m_comps_mutex;
		TypeGroup* m_type_group = nullptr;
		StorageBlock* m_storage_block = nullptr;
		size_t m_index = 0;
		mutable Potato::Tool::atomic_reference_count m_ref;
	};

	using EntityImpPtr = Potato::Tool::intrusive_ptr<EntityImp>;
}