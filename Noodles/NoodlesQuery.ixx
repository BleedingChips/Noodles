module;

#include <cassert>

export module NoodlesQuery;

import std;
import Potato;

import NoodlesArchetype;
import NoodlesBitFlag;
import NoodlesComponent;
//import NoodlesGlobalContext;

export namespace Noodles
{

	struct ComponentQuery : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<ComponentQuery>;

		BitFlagContainerConstViewer GetRequireComponentBitFlag() const { return require_component; }
		BitFlagContainerConstViewer GetRefuseComponentBitFlag() const { return refuse_component; }
		BitFlagContainerConstViewer GetArchetypeUsageArray() const { return archetype_usable; }

		static Ptr Create(
			std::size_t archetype_container_count, 
			std::size_t component_container_count, 
			std::span<BitFlag const> require_component_bitflag,
			std::span<BitFlag const> require_write_component_bitflag,
			std::span<BitFlag const> refuse_component_bitflag,
			std::pmr::memory_resource* resource = std::pmr::get_default_resource()
		);

		bool UpdateQueryData(ComponentManager const& manager);

		std::size_t GetArchetypeCount() const { return archetype_count; }

		std::optional<std::size_t> QueryComponentArrayWithIterator(ComponentManager& manager, std::size_t iterator, std::size_t chunk_index, std::span<void*> output_component);
		bool QueryComponent(ComponentManager& manager, ComponentManager::Index component_index, std::span<void*> output_component);

		~ComponentQuery() = default;

	protected:

		ComponentQuery(
			Potato::IR::MemoryResourceRecord record,
			BitFlagContainerConstViewer require_component,
			BitFlagContainerConstViewer require_write_component,
			BitFlagContainerConstViewer refuse_component,
			BitFlagContainerViewer archetype_usable,
			std::span<BitFlag const> component_bitflag
		)
			:MemoryResourceRecordIntrusiveInterface(record), 
			require_component(require_component),
			require_write_component(require_write_component),
			refuse_component(refuse_component),
			archetype_usable(archetype_usable),
			component_bitflag(component_bitflag),
			query_data(record.GetMemoryResource())
		{
			query_data_fast_offset = component_bitflag.size() * ComponentManager::GetQueryDataCount() + 1;
		}


		BitFlagContainerConstViewer require_component;
		BitFlagContainerConstViewer require_write_component;
		BitFlagContainerConstViewer refuse_component;
		BitFlagContainerViewer archetype_usable;
		std::span<BitFlag const> component_bitflag;
		std::pmr::vector<std::size_t> query_data;
		std::size_t archetype_count = 0;
		std::size_t updated_archetype_count = 0;
		std::size_t query_data_fast_offset = 0;

		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
	};





	/*
	struct ComponentQueryManager
	{

		void RegisterQueryIdentity(std::size_t query_identity);
		void UnRegisterQueryIdentity(std::size_t query_identity);

		bool GetQueryData(std::size_t query_identity, std::size_t archetype, std::span<BitFlag const> component_class, std::span<std::size_t> output_offset_and_size);
		bool UpdateFromComponent(ComponentManager& manager);

		ComponentQueryManager(GlobalContext::Ptr context, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

	protected:

		struct QueryInfo
		{
			std::size_t index = 0;
			std::size_t reference_count = 0;
			Potato::Misc::IndexSpan<> query_data;
			bool need_update = false;
		};

		GlobalContext::Ptr context;
		std::pmr::vector<QueryInfo> query_info;
		std::pmr::vector<BitFlag> container;
		std::pmr::vector<std::size_t> offset_or_size;
	};
	*/








	/*
	struct QueryData
	{
		QueryData(std::span<void*> output_buffer) : output_buffer(output_buffer) {}
		QueryData(QueryData const& other, std::span<void*> output_buffer) : archetype(other.archetype), array_size(other.array_size), output_buffer(output_buffer)
		{
			assert(output_buffer.size() == other.output_buffer.size());
			std::memcpy(output_buffer.data(), other.output_buffer.data(), sizeof(void*) * output_buffer.size());
		}
		template<typename Type>
		std::span<Type> AsSpan(std::size_t index) const {
			return std::span{
				static_cast<Type*>(output_buffer[index]),
				array_size
			};
		}

		Archetype::OPtr archetype;
		std::span<void*> output_buffer;
		std::size_t array_size = 0;
		operator bool() const { return archetype; }
	};

	

	struct SingletonQuery : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{

		using Ptr = Potato::Pointer::IntrusivePtr<SingletonQuery>;

		StructLayoutMarksInfosView GetRequiredStructLayoutMarks() const
		{
			return { require_write_singleton, require_singleton };
		}

		std::span<MarkIndex const> GetMarkIndex() const { return mark_index; }


		void Reset();

		static SingletonQuery::Ptr Create(
			StructLayoutManager& manager,
			std::span<StructLayoutWriteProperty const> require_singleton,
			std::pmr::memory_resource* storage_resource = std::pmr::get_default_resource()
		);

		std::span<std::size_t const> EnumSingleton_AssumedLocked() const { return archetype_offset; }
		bool OnSingletonModify_AssumedLocked(Archetype const& archetype);
		bool Update_AssumedLocked(StructLayoutManager& manager, std::size_t archetype_id);
		bool VersionCheck_AssumedLocked(std::size_t archetype_id) const;
		std::shared_mutex& GetMutex() const { return mutex; }

	protected:

		SingletonQuery(
			Potato::IR::MemoryResourceRecord record,
			std::span<MarkElement> require_singleton,
			std::span<MarkElement> require_write_singleton,
			std::span<MarkIndex> mark_index,
			std::span<std::size_t> archetype_offset,
			StructLayoutManager::Ptr manager
		)
			:MemoryResourceRecordIntrusiveInterface(record), require_singleton(require_singleton),
			require_write_singleton(require_write_singleton),
			mark_index(mark_index),
			archetype_offset(archetype_offset),
			manager(std::move(manager))
		{
		}


		StructLayoutManager::Ptr const manager;
		std::span<MarkElement> require_singleton;
		std::span<MarkElement> require_write_singleton;
		std::span<MarkIndex> mark_index;

		mutable std::shared_mutex mutex;
		std::span<std::size_t> archetype_offset;
		std::size_t archetype_id = 0;

		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
	};

	struct ThreadOrderQuery : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		StructLayoutMarksInfosView GetStructLayoutMarks() const { return marks; };
		using Ptr = Potato::Pointer::IntrusivePtr<ThreadOrderQuery>;

		static Ptr Create(StructLayoutManager& manager, std::span<StructLayoutWriteProperty const> info, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

	protected:
		ThreadOrderQuery(Potato::IR::MemoryResourceRecord record, StructLayoutMarksInfos marks)
			: MemoryResourceRecordIntrusiveInterface(record), marks(marks) {
		}
		StructLayoutMarksInfos marks;




		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
	};
	*/
}