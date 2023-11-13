module;

#include <cassert>
export module NoodlesArchetype;

import std;
import PotatoIR;
export import PotatoPointer;
import PotatoTaskSystem;

export namespace Noodles
{

	struct ArchetypeComponentManager;

	struct UniqueTypeID
	{
		Potato::IR::TypeID id;

		UniqueTypeID(Potato::IR::TypeID id) : id(id) {}
		UniqueTypeID(UniqueTypeID const&) = default;
		UniqueTypeID& operator=(UniqueTypeID const&) = default;
		

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
		bool operator==(UniqueTypeID const& i1) const { return this->operator<=>(i1) == std::strong_ordering::equivalent; }
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
			Destruction,
			DefaultConstruction
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
					case Status::DefaultConstruction:
						new (self) std::remove_cvref_t<Type>{};
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
		bool operator==(const ArchetypeID& i1) const { return this->operator<=>(i1) == std::strong_ordering::equivalent; }
	};

	struct ArchetypeMountPoint
	{
		void* buffer = nullptr;
		std::size_t element_count = 1;
		std::size_t index = 0;
		ArchetypeMountPoint& operator ++() { index += 1; return *this; }
		ArchetypeMountPoint& operator +=(std::size_t i) { index += i; return *this; }
		ArchetypeMountPoint& operator --() { assert(index > 0); index -= 1; return *this; }
		ArchetypeMountPoint& operator -=(std::size_t i) { index -= i; return *this; }
		std::strong_ordering operator<=>(ArchetypeMountPoint const& mp) const;
		bool operator==(ArchetypeMountPoint const& i2) const { return this->operator<=>(i2) == std::strong_ordering::equivalent; }
		operator bool() const;
		ArchetypeMountPoint& operator*(){ return *this; }
		ArchetypeMountPoint const& operator*() const { return *this; }
	};

	struct ArchetypeConstructor
	{
		ArchetypeConstructor(std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		bool AddElement(ArchetypeID const& id);

		bool AddElement(std::span<ArchetypeID const> span, std::size_t& bad_index);

		template<typename Type>
		bool AddElement() { return AddElement(ArchetypeID::Create<Type>()); }

		std::optional<std::size_t> Exits(UniqueTypeID const& id) const;

	protected:

		struct Element
		{
			ArchetypeID id;
			std::size_t count;

			Element(ArchetypeID const& id, std::size_t count) : id(id), count(count) {}
			Element(Element const&) = default;
			Element(Element &&) = default;
			Element& operator=(Element const&) = default;
		};

		std::pmr::vector<Element> elements;

		friend struct Archetype;
	};

	/*
	struct Archetype : public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<Archetype>;

		

		static Ptr Create(ArchetypeConstructor const& reference, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		struct Location
		{
			std::size_t index = 0;
			std::size_t count = 0;
		};

		std::optional<Location> LocateTypeID(UniqueTypeID const& type_id) const;

		std::size_t GetTypeIDCount() const { return infos.size(); }

		UniqueTypeID const& GetTypeID(std::size_t index) const;
		UniqueTypeID const& GetTypeID(std::size_t index, std::size_t& count) const;

		void* GetData(std::size_t locate_index, std::size_t count, ArchetypeMountPoint mount_point) const;

		void MoveConstruct(std::size_t locate_index, void* target, void* source) const;
		void Destruction(std::size_t locate_index, void* target) const;
		void DefaultConstruct(std::size_t locate_index, void* target) const;

		Archetype::Ptr Clone(std::pmr::memory_resource* o_resource) const;

		Potato::IR::Layout GetLayout() const { return archetype_layout; }
		Potato::IR::Layout GetBufferLayout() const { return { archetype_layout.Align, buffer_archetype_layout_size }; }
		std::strong_ordering operator<=>(Archetype const&) const;
		std::pmr::memory_resource* GetResource() const { return resource; }
		std::size_t GetFastIndex() const { return fast_index; }

	protected:

		
		Archetype(
			ArchetypeConstructor const& contructor,
			std::span<std::byte> buffer,
			std::pmr::memory_resource*, std::size_t allocated_size
			);

		Archetype(
			Archetype const& ref,
			std::span<std::byte> buffer,
			std::pmr::memory_resource*, std::size_t allocated_size
		);


		~Archetype();

		

		virtual void Release() override;


		struct Element
		{
			ArchetypeID id;
			std::size_t count = 1;
			std::size_t offset = 0;

			Element(ArchetypeID id, std::size_t count) : id(id), count(count){};
			Element(Element const&) = default;
			Element(Element &&) = default;
			Element& operator=(Element const&) = default;
			Element& operator=(Element&&) = default;
		};

		//std::pmr::memory_resource* resource = nullptr;
		//Potato::IR::Layout archetype_layout;
		//std::size_t buffer_archetype_layout_size = 0;
		//std::span<Element const> infos;
		//std::size_t allocated_size = 0;
		//std::size_t fast_index = 0;

		//friend struct ArchetypeComponentManager;
	};
	*/

}