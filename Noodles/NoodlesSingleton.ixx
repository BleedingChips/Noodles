module;

#include <cassert>

export module NoodlesSingleton;

import std;
import PotatoTMP;
import PotatoMisc;
import PotatoPointer;
import PotatoIR;
import PotatoTaskSystem;

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

	struct SingletonWrapper
	{
		std::pmr::vector<std::byte*> buffers;
		template<typename Type>
		Type* As(std::size_t index) const
		{
			auto b = reinterpret_cast<Type*>(buffers[index]);
			return (b == nullptr) ? nullptr : reinterpret_cast<Type*>(b);
		}
	};

	struct SingletonFilter : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{

		using Ptr = Potato::Pointer::IntrusivePtr<SingletonFilter>;

		StructLayoutMarksInfosView GetRequiredStructLayoutMarks() const
		{
			return { require_write_singleton, require_singleton };
		}

		std::span<MarkIndex const> GetMarkIndex() const { return mark_index; }

		bool OnSingletonModify(Archetype const& archetype);
		void Reset();

		static SingletonFilter::Ptr Create(
			StructLayoutMarkIndexManager& manager,
			std::span<StructLayoutWriteProperty const> require_singleton,
			std::pmr::memory_resource* storage_resource = std::pmr::get_default_resource()
		);

		std::span<std::size_t const> EnumSingleton_AssumedLocked() const { return archetype_offset; }

	protected:

		SingletonFilter(
			Potato::IR::MemoryResourceRecord record,
			std::span<MarkElement> require_singleton,
			std::span<MarkElement> require_write_singleton,
			std::span<MarkIndex> mark_index,
			std::span<std::size_t> archetype_offset
		)
			:MemoryResourceRecordIntrusiveInterface(record), require_singleton(require_singleton),
			require_write_singleton(require_write_singleton),
			mark_index(mark_index),
			archetype_offset(archetype_offset)
		{
		}

		std::span<MarkElement> require_singleton;
		std::span<MarkElement> require_write_singleton;
		std::span<MarkIndex> mark_index;

		mutable std::shared_mutex mutex;
		std::span<std::size_t> archetype_offset;

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

		SingletonManager(Config config = {});
		~SingletonManager();

		SingletonFilter::Ptr CreateSingletonFilter(std::span<StructLayoutWriteProperty const> input, std::size_t identity, std::pmr::memory_resource* filter_resource = std::pmr::get_default_resource());

		SingletonWrapper ReadSingleton_AssumedLocked(SingletonFilter const& filter, std::pmr::memory_resource* wrapper_resource = std::pmr::get_default_resource()) const;
		bool AddSingleton(StructLayout::Ptr struct_layout, void* target_buffer, EntityManager::Operation operation, std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());
		template<typename SingletonType>
		bool AddSingleton(SingletonType&& type, std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource())
		{
			return this->AddSingleton(StructLayout::GetStatic<SingletonType>(), &type, std::is_rvalue_reference_v<SingletonType&&> ? EntityManager::Operation::Move : EntityManager::Operation::Copy, temp_resource);
		}

		bool RemoveSingleton(StructLayout::Ptr const& atomic_type);
		bool Flush(std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());
		SingletonView GetSingletonView_AssumedLocked() const
		{
			return { singleton_archetype, singleton_record.GetByte() };
		}
		std::span<MarkElement const> GetSingletonUsageMark_AssumedLocked() const { return singleton_mark; }
		std::shared_mutex& GetMutex() { return mutex; }
		std::size_t GetSingletonMarkElementStorageCount() const { return manager.GetStorageCount(); }

	protected:

		bool ClearCurrentSingleton_AssumedLocked();

		StructLayoutMarkIndexManager manager;

		std::shared_mutex mutex;
		Archetype::Ptr singleton_archetype;
		Potato::IR::MemoryResourceRecord singleton_record;
		std::pmr::unsynchronized_pool_resource singleton_resource;
		std::pmr::vector<MarkElement> singleton_mark;

		struct FilterTuple
		{
			SingletonFilter::Ptr filter;
			OptionalIndex identity;
		};

		std::shared_mutex filter_mutex;
		std::pmr::vector<FilterTuple> filter;

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