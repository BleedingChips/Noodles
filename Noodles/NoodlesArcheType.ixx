module;

#include <cassert>
export module NoodlesArchetype;

import std;
import PotatoIR;
import PotatoPointer;
import PotatoTaskSystem;
import NoodlesMemory;

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

	struct Archetype : public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<Archetype>;

		static Ptr Create(std::span<ArchetypeID const> ref_info, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		struct LocateResult
		{
			Potato::IR::Layout layout;
			std::size_t offset;
		};

		std::optional<std::tuple<Potato::IR::Layout, std::size_t>> LocateType(UniqueTypeID unique_id) const;
		void* LocateType(UniqueTypeID unique_id, void* buffer, std::size_t index = 0, std::size_t total_index_count = 1) const;
		void* Copy(Potato::IR::TypeID unique_id, void* buffer, void* source, std::size_t index = 1, std::size_t total_index_count = 1) const;

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

	


	
	/*
	struct FilterWrapper : public Potato::Task::ControlDefaultInterface
	{
		using WPtr = Potato::Pointer::IntrusivePtr<FilterWrapper>;

		FilterWrapper(Potato::Pointer::IntrusivePtr<ArchetypeManager> owner)
			: owner(std::move(owner)) {}

	protected:

		Potato::Pointer::IntrusivePtr<ArchetypeManager> owner;

		std::pmr::vector<Archetype::Ptr> arche_type_reference;

		friend struct FilterWrapperManager;
	};*/


	/*
	struct ArchetypeManager : public Potato::Intrusice
	{

		struct Element
		{
			Archetype::Ptr archetype_ptr;
			std::size_t totale_entity_count;
			Memory::ComponentPage::SPtr top_page;
			Memory::ComponentPage::SPtr last_page;
		};

		//std::vector<Element> ;

		struct FilterElement
		{
			std::pmr::vector<Potato::IR::TypeID> ids;
		};
	};
	*/



	/*
	struct Union : public Potato::Pointer::DefaultIntrusiveInterface
	{

	};
	*/


	/*
	struct TypeInfo
	{
		using Layout = Potato::IR::Layout;

		enum class MethodT
		{
			Destruct,
			MoveContruct,
		};

		enum class PropertyT
		{
			Static,
			Dynamic
		};

		template<typename Type>
		static TypeInfo Get() {
			using RT = std::remove_cv_t<std::remove_reference_t<Type>>;
			return {
				PropertyT::Static,
				typeid(RT).hash_code(),
				Layout::Get<RT>(),
				[](MethodT Method, std::byte* Target, std::byte* Source) {
					switch (Method)
					{
					case MethodT::Destruct:
						reinterpret_cast<RT*>(Target)->~RT();
						break;
					case MethodT::MoveContruct:
						new (Target) RT{std::move(*reinterpret_cast<RT*>(Source))};
						break;
					}
				}
			};
		};

		TypeInfo(PropertyT Pro, std::size_t HashCode, Layout TypeLayout, void (*MethodFunction) (MethodT, std::byte*, std::byte*))
			: Property(Pro), HashCode(HashCode), TypeLayout(TypeLayout), MethodFunction(MethodFunction) {}
		TypeInfo(TypeInfo const&) = default;

		void MoveContruct(std::byte* Target, std::byte* Source) const {  (*MethodFunction)(MethodT::MoveContruct, Target, Source); }

		void Destruct(std::byte* Target) const { (*MethodFunction)(MethodT::MoveContruct, Target, nullptr); }

		std::size_t GetHashCode() const { return HashCode; }
		Layout const& GetLayout() const { return TypeLayout; }

	private:
		
		PropertyT Property;
		std::size_t HashCode;
		Layout TypeLayout;
		void (*MethodFunction) (MethodT Met, std::byte* Target, std::byte* Source);
	};

	struct Group
	{
		using Layout = TypeInfo::Layout;
		Layout TotalLayout;
		std::span<std::tuple<std::size_t, TypeInfo>> Infos;
	};
	*/

}