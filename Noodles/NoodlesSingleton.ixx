module;

#include <cassert>

export module NoodlesSingleton;

import std;
import Potato;

export import NoodlesArchetype;
import NoodlesComponent;
import NoodlesEntity;
import NoodlesQuery;

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

		bool ReadSingleton_AssumedLocked(SingletonQuery const& query, QueryData& accessor) const;
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
		bool UpdateFilter_AssumedLocked(SingletonQuery& query) const;

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