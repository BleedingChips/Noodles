module;

#include <cassert>
export module NoodlesArchetype;

import std;
import PotatoIR;
import PotatoPointer;
import PotatoTaskSystem;

export namespace Noodles
{

	struct ArchetypeComponentManager;

	struct UniqueTypeID
	{

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

		std::strong_ordering operator<=>(UniqueTypeID const& i1) const { return id <=> i1.id; }
		bool operator==(const UniqueTypeID& i1) const { return id == i1.id; }
		decltype(auto) HashCode() const {return id.HashCode(); }

	protected:
		Potato::IR::TypeID id;
	};

	/*
	template<typename Type>
	struct ThreadSafeMarker
	{
		static constexpr bool value = false;
	};

	template<typename Type>
	concept HasThreadSafeMarker = requires(Type t)
	{
		typename Type::NoodlesThreadSafeMarker;
	};

	template<HasThreadSafeMarker Type>
	struct ThreadSafeMarker<Type>
	{
		static constexpr bool value = true;
	};
	*/

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
		//bool thread_safe = false;
		void (*wrapper_function)(Status status, void* self, void* target) = nullptr;
		

		ArchetypeID(
			UniqueTypeID id,
			Potato::IR::Layout layout,
			//bool thread_safe,
			void (*wrapper_function)(Status status, void* self, void* target)
		)
			: id(id), layout(layout), wrapper_function(wrapper_function)
		{
			
		}

		ArchetypeID(ArchetypeID const&) = default;
		ArchetypeID& operator=(ArchetypeID const&) = default;

		template<typename Type>
		static ArchetypeID Create()
		{
			static ArchetypeID id{
				UniqueTypeID::Create<Type>(),
				Potato::IR::Layout::Get<Type>(),
				//ThreadSafeMarker<Type>::value,
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
				}
			};
			return id;
		}

		std::strong_ordering operator<=>(ArchetypeID const& i1) const;
		bool operator==(const ArchetypeID& i1) const;
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
		bool operator==(ArchetypeMountPoint const& i2) const { return buffer == i2.buffer; }
		operator bool() const;
		ArchetypeMountPoint& operator*(){ return *this; }
		ArchetypeMountPoint const& operator*() const { return *this; }
		void* GetBuffer(std::size_t offset) const { return static_cast<std::byte*>(buffer) + offset * element_count;}
	};


	struct Archetype : public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<Archetype>;

		static Ptr Create(std::span<ArchetypeID const> id, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		std::optional<std::size_t> LocateTypeID(UniqueTypeID const& type_id) const;
		std::optional<std::size_t> LocateTypeID(ArchetypeID const& type_id) const { return LocateTypeID(type_id.id); }

		std::size_t GetTypeIDCount() const { return infos.size(); }

		UniqueTypeID const& GetTypeID(std::size_t index) const;

		void* GetData(std::size_t locate_index, ArchetypeMountPoint mount_point) const;

		void MoveConstruct(std::size_t locate_index, void* target, void* source) const;
		void Destruction(std::size_t locate_index, void* target) const;
		void DefaultConstruct(std::size_t locate_index, void* target) const;
		void Destruction(ArchetypeMountPoint mount_point) const;
		void MoveConstruct(ArchetypeMountPoint target_mp, ArchetypeMountPoint source_mp) const;

		Archetype::Ptr Clone(std::pmr::memory_resource* o_resource) const;
		std::size_t GetHashCode() const { return type_id_hash_code; }

		Potato::IR::Layout GetSingleLayout() const { return single_layout; }
		Potato::IR::Layout GetArchetypeLayout() const { return archetype_layout; }
		//std::strong_ordering operator<=>(Archetype const&) const;
		bool operator==(Archetype const& ar) const;
		std::pmr::memory_resource* GetResource() const { return record.GetResource(); }
		std::size_t GetElementCount() const { return infos.size(); }

		static bool CheckUniqueArchetypeID(std::span<ArchetypeID const>);

	protected:

		struct Element
		{
			ArchetypeID id;
			std::size_t offset = 0;

			Element(ArchetypeID id, std::size_t offset) : id(id), offset(offset) {};
			Element(Element const&) = default;
			Element(Element&&) = default;
			Element& operator=(Element const&) = default;
			Element& operator=(Element&&) = default;
			bool operator==(Element const& i) const { return offset == i.offset && id == i.id; }
		};

		Archetype(
			Potato::IR::MemoryResourceRecord record,
			std::span<Element> infos
			);

		Archetype(
			Archetype const& ref,
			std::span<Element> infos,
			Potato::IR::MemoryResourceRecord record
		);


		~Archetype();

		virtual void Release() override;

		Potato::IR::MemoryResourceRecord record;
		std::size_t type_id_hash_code = 0;
		Potato::IR::Layout single_layout;
		Potato::IR::Layout archetype_layout;
		std::span<Element> infos;

		friend struct ArchetypeComponentManager;
	};

}