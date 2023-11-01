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
		Potato::IR::Layout layout;

		template<typename Type>
		static UniqueTypeID Create()
		{
			return {
				Potato::IR::TypeID::CreateTypeID<Type>(),
				Potato::IR::Layout::Get<Type>()
			};
		}

		friend std::strong_ordering operator<=>(UniqueTypeID const& i1, UniqueTypeID const& i2);
	};

	std::strong_ordering operator<=>(UniqueTypeID const& i1, UniqueTypeID const& i2);

	struct ArchetypeID
	{
		enum class Status
		{
			MoveConstruction,
			Destruction
		};

		UniqueTypeID id;
		void (*WrapperFunction)(Status status, void* self, void* target) = nullptr;

		template<typename Type>
		static ArchetypeID Create()
		{
			return {
				UniqueTypeID::Create<Type>(),
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
				}
			};
		}

		friend std::strong_ordering operator<=>(ArchetypeID const& i1, ArchetypeID const& i2);
	};

	inline std::strong_ordering operator<=>(ArchetypeID const& i1, ArchetypeID const& i2)
	{
		return i1.id <=> i2.id;
	}

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

		static Ptr Create(std::span<ArchetypeID const> ref_info, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		std::optional<std::size_t> LocateFirstTypeID(UniqueTypeID const& type_id) const;
		std::size_t GetTypeIDCount() const { return infos.size(); }
		UniqueTypeID const& GetTypeID(std::size_t index) const;
		void* GetData(std::size_t locate_index, ArchetypeMountPoint mount_point) const;
		void MoveConstruct(std::size_t locate_index, void* target, void* source) const;
		void Destruction(std::size_t locate_index, void* target) const;

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