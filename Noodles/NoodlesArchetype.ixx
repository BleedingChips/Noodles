module;

#include <cassert>
export module NoodlesArchetype;

import std;
import PotatoIR;
import PotatoPointer;
import PotatoTaskSystem;


export namespace Noodles
{

	struct UniqueTypeID
	{
		Potato::IR::TypeID id;
		

		template<typename Type>
		static UniqueTypeID Create()
		{
			return {
				Potato::IR::TypeID::CreateTypeID<Type>()
			};
		}

		std::strong_ordering operator<=>(UniqueTypeID const& i1) const
		{
			return id <=> i1.id;
		}
		bool operator==(UniqueTypeID const& i1) const = default;
	};

	template<typename Type>
	struct SingletonRequire
	{
		static constexpr bool value = false;
	};

	template<typename Type>
	concept IsSingletonRequire = requires(Type t)
	{
		typename Type::NoodlesSingletonRequire;
	};

	template<IsSingletonRequire Type>
	struct SingletonRequire<Type>
	{
		static constexpr bool value = true;
	};

	

	struct ArchetypeID
	{
		enum class Status
		{
			MoveConstruction,
			Destruction
		};

		UniqueTypeID id;
		Potato::IR::Layout layout;
		void (*wrapper_function)(Status status, void* self, void* target) = nullptr;
		bool is_singleton = false;

		template<typename Type>
		static ArchetypeID Create()
		{
			return {
				UniqueTypeID::Create<Type>(),
				Potato::IR::Layout::Get<Type>(),
				[](Status status, void* self, void* target)
				{
					switch(status)
					{
					case Status::Destruction:
						static_cast<std::remove_cvref_t<Type>*>(self)->~Type();
						break;
					case Status::MoveConstruction:
						new (self) std::remove_cvref_t<Type>{std::move(*static_cast<std::remove_cvref_t<Type>*>(target))};
						break;
					default:
						assert(false);
						break;
					}
				},
				SingletonRequire<Type>::value
			};
		}

		std::strong_ordering operator<=>(ArchetypeID const& i1) const;
	};

	struct ArchetypeMountPoint
	{
		void* buffer = nullptr;
		std::size_t element_count = 1;
		std::size_t index = 0;
		operator bool() const;
	};

	struct Archetype : public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<Archetype>;

		static Ptr Create(std::span<ArchetypeID const> ref_info, std::span<ArchetypeID const> append_info = {}, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		std::optional<std::size_t> LocateTypeID(UniqueTypeID const& type_id, std::size_t index = 0) const;
		std::size_t GetTypeIDCount() const { return infos.size(); }
		UniqueTypeID const& GetTypeID(std::size_t index) const;
		void* GetData(std::size_t locate_index, ArchetypeMountPoint mount_point) const;
		void MoveConstruct(std::size_t locate_index, void* target, void* source) const;
		void Destruction(std::size_t locate_index, void* target) const;
		Archetype::Ptr Clone(std::pmr::memory_resource* o_resource) const;
		Potato::IR::Layout GetLayout() const { return archetype_layout; }


	protected:

		Archetype(std::pmr::memory_resource*, std::size_t allocated_size);

		virtual void Release() override;

		struct Element
		{
			ArchetypeID id;
			std::size_t offset;

			Element(ArchetypeID id) : id(id) {};
			Element(Element const&) = default;
			Element(Element &&) = default;
			Element& operator=(Element const&) = default;
			Element& operator=(Element&&) = default;
		};

		Potato::IR::Layout archetype_layout;
		std::span<Element const> infos;
		std::pmr::memory_resource* resource;
		std::size_t allocated_size = 0;
		

		friend struct ArchetypeManager;

		friend std::strong_ordering operator<=>(Archetype const& i1, Archetype const& i2);
	};

	std::strong_ordering operator<=>(Archetype const& i1, Archetype const& i2);

	struct TypeInfo
	{
		Potato::IR::TypeID ID;
		Potato::IR::Layout Layout;
		void (*MoveConstructor)(void* Source, void* Target);
		void (*Destructor)(void* Target);

		template<typename Type>
		static TypeInfo Create()
		{
			return {
			Potato::IR::TypeID::CreateTypeID<Type>(),
				Potato::IR::Layout::Get<Type>(),
				[](void* Source, void* Target) { new (Target) Type{ std::move(*static_cast<Type*>(Source)) }; },
				[](void* Target) { static_cast<Type*>(Target)->~Type(); }
			};
		}
	};

}