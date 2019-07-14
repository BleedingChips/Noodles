#pragma once
#include "entity.h"
#include "memory.h"
#include <set>
#include <deque>
#include <variant>
namespace Noodles::Implement
{
	struct TypeLayoutArray
	{
		const TypeInfo* layouts = nullptr;
		size_t count = 0;
		bool operator<(const TypeLayoutArray&) const noexcept;
		bool operator==(const TypeLayoutArray&) const noexcept;
		TypeLayoutArray& operator=(const TypeLayoutArray&) = default;
		TypeLayoutArray(const TypeLayoutArray&) = default;
		const TypeInfo& operator[](size_t index) const noexcept
		{
			assert(index < count);
			return layouts[index];
		}

		size_t locate(const TypeInfo& input) const noexcept;
		bool locate_ordered(const TypeInfo* input, size_t* output, size_t length) const noexcept;
		bool locate_unordered(const TypeInfo* input, size_t* output, size_t length) const noexcept;

		bool hold(const TypeInfo& input) const noexcept { return (locate(input) < count); }
		bool hold_ordered(const TypeLayoutArray& array) const noexcept { return hold_ordered(array.layouts, array.count); }
		bool hold_ordered(const TypeInfo* input, size_t count) const noexcept { return locate_ordered(input, nullptr, count); }
		bool hold_unordered(const TypeLayoutArray& array) const noexcept { return hold_unordered(array.layouts, array.count); }
		bool hold_unordered(const TypeInfo* input, size_t length) const noexcept { return locate_unordered(input, nullptr, length); }
	};

	struct TypeGroup
	{
		TypeLayoutArray layouts() const noexcept { return m_type_layouts; }
		static TypeGroup* create(TypeLayoutArray array);
		static void free(TypeGroup*);

		size_t element_count() const noexcept { return m_element_count; }
		size_t page_size() const noexcept { return m_page_size; }
		
		std::tuple<StorageBlock*, size_t> allocate_group(MemoryPageAllocator& allocator);
		void release_group(StorageBlock* block, size_t);
		void update();
		StorageBlock* top_block() const noexcept { return m_start_block; }
		size_t available_count() const noexcept { return m_available_count; }

	private:

		void remove_page_from_list(StorageBlock*);
		void insert_page_to_list(StorageBlock*);
		void inside_move(StorageBlock* source, size_t sindex, StorageBlock* target, size_t tindex);

		TypeGroup(TypeLayoutArray);
		~TypeGroup();
		
		TypeLayoutArray m_type_layouts;
		StorageBlock* m_start_block = nullptr;
		StorageBlock* m_last_block = nullptr;

		size_t m_page_size;
		size_t m_element_count;
		size_t m_available_count = 0;
		std::map<StorageBlock*, size_t> m_deleted_page;
	};

	struct InitHistory
	{
		bool is_construction;
		TypeInfo type;
		void* data;
	};

	struct ComponentPool : ComponentPoolInterface
	{
		std::shared_mutex& read_mutex() noexcept { return m_type_group_mutex; }
		virtual size_t type_group_count() const noexcept override;
		virtual void search_type_group(
			const TypeInfo* require_tl, size_t require_tl_count,
			TypeGroup** output_tg,
			size_t* output_tl_index
		) const noexcept override;

		virtual void handle_entity_imp(EntityInterface*, EntityOperator ope) noexcept override;
		virtual void construct_component(
			const TypeInfo& layout, void(*constructor)(void*, void*), void* data,
			EntityInterface*, void(*deconstructor)(void*) noexcept, void(*mover)(void*, void*) noexcept
		) override;
		virtual size_t find_top_block(TypeGroup** tg, StorageBlock ** output, size_t length) const noexcept override;
		virtual void deconstruct_component(EntityInterface*, const TypeInfo& layout) noexcept override;
		bool update();
		void update_type_group_state(std::vector<bool>& ite);
		void clean_all();
		ComponentPool(MemoryPageAllocator& allocator) noexcept;
		~ComponentPool();

	private:

		struct InitBlock
		{
			std::byte* start_block;
			void* last_block;
			size_t last_available_count;
			InitBlock(std::byte* sb, void* last, size_t l) noexcept
				: start_block(sb), last_block(last), last_available_count(l) {}
			InitBlock(InitBlock&& ib) noexcept : start_block(ib.start_block), last_block(ib.last_block), last_available_count(ib.last_available_count) {
				ib.start_block = nullptr;
			}
			~InitBlock();
		};

		struct InitHistory
		{
			EntityOperator ope;
			TypeInfo type;
			StorageBlockFunctionPair functions;
			void* data;
			InitHistory(EntityOperator i, const TypeInfo& t, StorageBlockFunctionPair p, void* d)
				: ope(i), type(t), functions(p), data(d) {}
			~InitHistory();
		};

		std::shared_mutex m_type_group_mutex;
		MemoryPageAllocator& m_allocator;
		std::map<TypeLayoutArray, TypeGroup*> m_data;

		std::mutex m_init_lock;
		std::vector<InitBlock> m_init_block;
		std::map<EntityInterfacePtr, std::vector<InitHistory>> m_init_history;
		
	};
}