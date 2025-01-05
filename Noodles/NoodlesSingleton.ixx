module;

#include <cassert>

export module NoodlesSingleton;

import std;
import Potato;

export import NoodlesArchetype;
import NoodlesComponent;
import NoodlesEntity;

export namespace Noodles
{

	export struct SingletonManager;
	struct SingletonView
	{
		Archetype::OPtr archetype;
		std::byte* buffer;
		std::byte* GetSingleton(
			Archetype::MemberView const& view
		) const;
		std::byte* GetSingleton(
			Archetype::Index index
		) const;
		std::byte* GetSingleton(
			MarkIndex index
		) const;
	};

	struct SingletonAccessor
	{
		SingletonAccessor(SingletonAccessor const& in, std::span<void*> in_buffer) : buffers(in_buffer) { assert(in.buffers.size() == in_buffer.size()); std::memcpy(buffers.data(), in.buffers.data(), sizeof(void*) * buffers.size()); }
		SingletonAccessor(std::span<void*> in) :buffers(in) {}
		
		template<typename Type>
		Type* As(std::size_t index) const
		{
			auto b = static_cast<Type*>(buffers[index]);
			return (b == nullptr) ? nullptr : reinterpret_cast<Type*>(b);
		}
	protected:
		std::span<void*> buffers;

		friend SingletonManager;
	};

	struct SingletonFilter : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{

		using Ptr = Potato::Pointer::IntrusivePtr<SingletonFilter>;

		StructLayoutMarksInfosView GetRequiredStructLayoutMarks() const
		{
			return { require_write_singleton, require_singleton };
		}

		std::span<MarkIndex const> GetMarkIndex() const { return mark_index; }

		
		void Reset();

		static SingletonFilter::Ptr Create(
			StructLayoutManager& manager,
			std::span<StructLayoutWriteProperty const> require_singleton,
			std::pmr::memory_resource* storage_resource = std::pmr::get_default_resource()
		);

		std::span<std::size_t const> EnumSingleton_AssumedLocked() const { return archetype_offset; }
		bool OnSingletonModify_AssumedLocked(Archetype const& archetype);

	protected:

		SingletonFilter(
			Potato::IR::MemoryResourceRecord record,
			std::span<MarkElement> require_singleton,
			std::span<MarkElement> require_write_singleton,
			std::span<MarkIndex> mark_index,
			std::span<std::size_t> archetype_offset,
			std::size_t struct_layout_manager_id
		)
			:MemoryResourceRecordIntrusiveInterface(record), require_singleton(require_singleton),
			require_write_singleton(require_write_singleton),
			mark_index(mark_index),
			archetype_offset(archetype_offset),
			struct_layout_manager_id(struct_layout_manager_id)
		{
		}


		std::size_t const struct_layout_manager_id;
		std::span<MarkElement> require_singleton;
		std::span<MarkElement> require_write_singleton;
		std::span<MarkIndex> mark_index;

		mutable std::shared_mutex mutex;
		std::span<std::size_t> archetype_offset;
		std::size_t singleton_manager_id = 0;
		std::size_t archetype_id = 0;

		friend struct SingletonManager;
		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
	};

	export struct SingletonManager
	{

		struct Config
		{
			std::size_t singleton_max_atomic_count = 128;
			std::pmr::memory_resource* singleton_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* resource = std::pmr::get_default_resource();
		};

		SingletonManager(StructLayoutManager& manager, Config config = {});
		~SingletonManager();

		bool ReadSingleton_AssumedLocked(SingletonFilter const& filter, SingletonAccessor& accessor) const;
		bool AddSingleton(StructLayout const& struct_layout, void* target_buffer, EntityManager::Operation operation, std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());
		template<typename SingletonType>
		bool AddSingleton(SingletonType&& type, std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource())
		{
			return this->AddSingleton(*StructLayout::GetStatic<SingletonType>(), &type, std::is_rvalue_reference_v<SingletonType&&> ? EntityManager::Operation::Move : EntityManager::Operation::Copy, temp_resource);
		}

		bool RemoveSingleton(StructLayout const& atomic_type);
		bool Flush(std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());
		SingletonView GetSingletonView_AssumedLocked() const
		{
			return { singleton_archetype, singleton_record.GetByte() };
		}
		std::span<MarkElement const> GetSingletonUsageMark_AssumedLocked() const { return singleton_mark; }
		bool HasSingletonUpdate_AssumedLocked() const { return has_singleton_update; }
		std::shared_mutex& GetMutex() { return mutex; }
		std::size_t GetSingletonMarkElementStorageCount() const { return manager->GetSingletonStorageCount(); }
		bool UpdateFilter_AssumedLocked(SingletonFilter& filter);

	protected:

		bool ClearCurrentSingleton_AssumedLocked();

		StructLayoutManager::Ptr manager;

		std::shared_mutex mutex;
		bool has_singleton_update = false;
		Archetype::Ptr singleton_archetype;
		Potato::IR::MemoryResourceRecord singleton_record;
		std::pmr::unsynchronized_pool_resource singleton_resource;
		std::pmr::vector<MarkElement> singleton_mark;

		struct Modify
		{
			MarkIndex mark_index;
			Potato::IR::MemoryResourceRecord resource;
			StructLayout::Ptr struct_layout;
			bool Release();
		};

		std::shared_mutex modifier_mutex;
		std::pmr::vector<MarkElement> modify_mask;
		std::pmr::vector<Modify> modifier;
		
	}; 
}