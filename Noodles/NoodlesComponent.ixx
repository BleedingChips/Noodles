module;

#include <cassert>

export module NoodlesComponent;

import std;
import PotatoMisc;
import PotatoPointer;
import PotatoIR;
import PotatoTaskSystem;

import NoodlesMemory;
export import NoodlesArchetype;
export import NoodlesEntity;

export namespace Noodles
{

	struct ComponentPage : public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<ComponentPage>;

		static auto Create(
			Potato::IR::Layout component_layout, std::size_t min_element_count, 
			std::size_t min_page_size, std::pmr::memory_resource* up_stream
		) -> Ptr;

		virtual void Release() override;

	

		ArchetypeMountPoint begin() const
		{
			return {
				buffer.data(),
				max_element_count,
				0
			};
		}
		ArchetypeMountPoint end() const
		{
			return {
				buffer.data(),
				max_element_count,
				available_count
			};
		}

		ArchetypeMountPoint GetLastMountPoint() const
		{
			return end();
		}

		ArchetypeMountPoint GetMaxMountPoint() const
		{
			return {
				buffer.data(),
				max_element_count,
				max_element_count
			};
		}

		Ptr GetNextPage() const { return next_page; }

	protected:

		ComponentPage(
			std::size_t max_element_count,
			std::size_t allocate_size,
			std::pmr::memory_resource* upstream,
			std::span<std::byte> buffer
		);

		virtual ~ComponentPage() = default;

		Ptr next_page;

		std::size_t const max_element_count = 0;
		std::size_t available_count = 0;
		std::span<std::byte> const buffer;
		std::size_t const allocate_size = 0;
		std::pmr::memory_resource* const resource = nullptr;

		friend struct ArchetypeComponentManager;
	};

	struct EntityFlags
	{
		std::pmr::vector<std::size_t> flags;
	};

	

	struct EntityConstructor
	{

		enum class Status
		{
			Done,
			BadArchetype,
			BadMemoryResource
		};

		template<typename Type>
		bool Construct(Type&& type, std::size_t i = 0)
		{
			return Construct(UniqueTypeID::Create<std::remove_cvref_t<Type>>(), &type, i);
		}

		bool Construct(UniqueTypeID const& id, void* source, std::size_t count = 0);

		operator bool() const { return status == Status::Done; }

	protected:

		EntityConstructor(EntityConstructor&&) = default;

		EntityConstructor(Status status) : status(status) {}

		EntityConstructor(
			Status status,
			Archetype::Ptr archetype_ptr,
			ArchetypeMountPoint mount_point,
			std::pmr::memory_resource* resource
		);

		Status status = Status::Done;
		Archetype::Ptr archetype_ptr;
		ArchetypeMountPoint mount_point;

		struct InitBit
		{
			std::size_t index;
			std::size_t count;
			bool operator==(InitBit const& ib) const{ return index == ib.index && count == ib.count; }
		};

		std::pmr::vector<InitBit> construct_record;
		std::size_t entity_property_index = 0;

		friend struct ArchetypeComponentManager;
	};

	struct ArchetypeComponentManager;

	struct ArchetypeMountPointRange
	{
		Archetype const& archetype;

		ArchetypeMountPoint begin() const { return mp_begin; }
		ArchetypeMountPoint end() const { return mp_end; }

		ArchetypeMountPoint mp_begin;
		ArchetypeMountPoint mp_end;
	};

	struct ComponentFilterInterface
	{

		using Ptr = Potato::Pointer::IntrusivePtr<ComponentFilterInterface>;

		virtual ~ComponentFilterInterface() = default;

	protected:

		virtual bool TryPreCollection(std::size_t element_index, Archetype const& archetype) = 0;
		virtual void AddRef() const = 0;
		virtual void SubRef() const = 0;

		friend struct Potato::Pointer::IntrusiveSubWrapperT;
		friend struct ArchetypeComponentManager;
	};

	struct ArchetypeComponentManager
	{

		struct EntityProperty
		{
			Entity entity;
		};

		ArchetypeComponentManager(std::pmr::memory_resource* upstream = std::pmr::get_default_resource());
		~ArchetypeComponentManager();

		ArchetypeConstructor CreateArchetypeConstructor(std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		template<typename Func>
		Entity CreateEntityDefer(std::span<ArchetypeID const> span, Func&& func, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
			requires(std::is_invocable_v<Func, EntityConstructor&>)
		{
			auto Constructor = PreCreateEntityImp(span, resource);
			if(Constructor)
			{
				func(Constructor);
				return CreateEntityImp(Constructor);
			}
			return {};
		}

		bool UpdateEntityStatus();
		bool DestroyEntity(Entity entity);
		bool RegisterComponentFilter(ComponentFilterInterface::Ptr ptr, std::size_t group_id);
		std::size_t ErasesComponentFilter(std::size_t group_id);
		std::size_t ArchetypeCount() const;

	protected:

		static ArchetypeID const& EntityPropertyArchetypeID();

		static void ReleaseEntity(EntityStorage& storage);

		EntityConstructor PreCreateEntityImp(std::span<ArchetypeID const> span, std::pmr::memory_resource* resource);
		Entity CreateEntityImp(EntityConstructor& constructor);

		struct Element
		{
			Archetype::Ptr archetype;
			ComponentPage::Ptr top_page;
			ComponentPage::Ptr last_page;
		};

		mutable std::shared_mutex components_mutex;
		std::pmr::vector<Element> components;
		std::pmr::synchronized_pool_resource components_resource;

		std::mutex spawn_mutex;
		std::pmr::vector<Entity> spawned_entities;
		std::pmr::monotonic_buffer_resource spawned_entities_resource;
		bool need_update = false;

		struct RemoveEntity
		{
			Entity entity;
			EntityStatus last_status;
		};

		std::pmr::vector<RemoveEntity> removed_entities;

		std::mutex archetype_resource_mutex;
		std::pmr::monotonic_buffer_resource archetype_resource;

		

		struct CompFilterElement
		{
			ComponentFilterInterface::Ptr filter;
			std::size_t group_id;
		};

		std::mutex filter_mapping_mutex;
		std::pmr::vector<CompFilterElement> filter_mapping;

		Memory::IntrusiveMemoryResource<std::pmr::synchronized_pool_resource>::Ptr entity_resource;

		friend struct EntityConstructor;
		friend struct ComponentFilterInterface;

	public:

		template<typename Func>
		bool ForeachMountPoint(std::size_t element_index, Func&& func) const
			requires(std::is_invocable_v<Func, ArchetypeMountPointRange&>)
		{
			std::shared_lock sl(components_mutex);
			if(element_index < components.size())
			{
				auto& ite = components[element_index];

				auto top = ite.top_page;

				while(top)
				{
					ArchetypeMountPointRange range{
						*ite.archetype, top->begin(), top->end()
					};
					func(range);
					top = top->GetNextPage();
				}
				return true;
			}
			return false;
		}

		template<typename FilterFunc, typename Func>
		bool ForeachMountPoint(std::size_t element_index, FilterFunc&& filter_func, Func&& func) const
			requires(
				std::is_invocable_r_v<bool, FilterFunc, Archetype const&>
				&& std::is_invocable_v<Func, ArchetypeMountPointRange&>
				)
		{
			std::shared_lock sl(components_mutex);
			if (element_index < components.size())
			{
				auto& ite = components[element_index];

				if(filter_func(*ite.archetype))
				{
					auto top = ite.top_page;

					while (top)
					{
						ArchetypeMountPointRange range{
							*ite.archetype, top->begin(), top->end()
						};
						func(range);
						top = top->GetNextPage();
					}
				}
				return true;
			}
			return false;
		}
	};


	

	template<typename ...Components>
	struct ComponentFilter
	{
		static_assert(sizeof...(Components) >= 1, "Component Filter Require At Least One Component");
	};

	template<typename GlobalComponent>
	struct GlobalComponentFilter
	{
		
	};

	export template<typename T>
	struct IsAcceptableComponentFilter
	{
		static constexpr bool Value = false;
	};

	export template<typename ...T>
	struct IsAcceptableComponentFilter<ComponentFilter<T...>>
	{
		static constexpr bool Value = true;
	};

	export template<typename T>
	constexpr bool IsAcceptableComponentFilterV = IsAcceptableComponentFilter<T>::Value;

	/*
	template<typename T>
	struct IsAcceptableGobalComponentFilter
	{
		static constexpr bool Value = false;
	};
	*/

}