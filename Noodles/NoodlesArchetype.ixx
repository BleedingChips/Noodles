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

	struct Archetype : public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<Archetype>;
		using OPtr = Potato::Pointer::ObserverPtr<Archetype const>;



		static Ptr Create(std::span<ArchetypeID const> id, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		std::optional<std::size_t> LocateTypeID(UniqueTypeID const& type_id) const;
		std::optional<std::size_t> LocateTypeID(ArchetypeID const& type_id) const { return LocateTypeID(type_id.id); }

		std::size_t GetTypeIDCount() const { return infos.size(); }

		UniqueTypeID const& GetTypeID(std::size_t index) const;

		Archetype::Ptr Clone(std::pmr::memory_resource* o_resource) const;
		std::size_t GetHashCode() const { return type_id_hash_code; }

		Potato::IR::Layout GetSingleLayout() const { return single_layout; }
		Potato::IR::Layout GetArchetypeLayout() const { return archetype_layout; }
		//std::strong_ordering operator<=>(Archetype const&) const;
		bool operator==(Archetype const& ar) const;
		std::pmr::memory_resource* GetResource() const { return record.GetMemoryResource(); }
		std::size_t GetElementCount() const { return infos.size(); }

		static bool CheckUniqueArchetypeID(std::span<ArchetypeID const>);

		struct ArrayMountPoint
		{
			void* archetype_array_buffer = nullptr;
			std::size_t available_count = 0;
			std::size_t total_count = 0;
			operator bool() const { return archetype_array_buffer != nullptr; }
			void* GetBuffer() const { return archetype_array_buffer; }
		};

		struct MountPoint
		{
			ArrayMountPoint array_mp;
			std::size_t array_mp_index;
			operator bool() const { return array_mp && array_mp_index < array_mp.available_count; }
			void* GetBuffer() const { return array_mp.GetBuffer(); }
		};

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

		struct RawArray
		{
			void* buffer;
			std::size_t array_count;
			Potato::IR::Layout element_layout;

			template<typename Type>
			std::span<Type> Translate() const
			{
				assert(element_layout.Align == alignof(Type) && element_layout.Size == sizeof(Type));
				return std::span(static_cast<Type*>(buffer), array_count);
			}
		};

		static void* Get(RawArray raw_data, std::size_t array_index);
		static RawArray Get(Element const& ref, ArrayMountPoint mount_point);
		static void* Get(Element const& ref, ArrayMountPoint mount_point, std::size_t array_index) { return  Get(Get(ref,mount_point), array_index); }
		static void* Get(Element const& ref, MountPoint mount_point) { return  Get(ref, mount_point.array_mp, mount_point.array_mp_index); }

		RawArray Get(std::size_t index, ArrayMountPoint mount_point) const { return Get(infos[index], mount_point); }
		void* Get(std::size_t index, ArrayMountPoint mount_point, std::size_t mount_point_index) const { return Get(infos[index], mount_point, mount_point_index); }


		static void MoveConstruct(Element const& el, void* target, void* source) { el.id.wrapper_function(ArchetypeID::Status::MoveConstruction, target, source); }
		static void MoveConstruct(Element const& el, RawArray const& target, std::size_t target_index, RawArray const& source, std::size_t source_index) { MoveConstruct(el, Get(target, target_index), Get(source, source_index)); }
		static void MoveConstruct(Element const& el, ArrayMountPoint const& target, std::size_t target_index, ArrayMountPoint const& source, std::size_t source_index) { assert(target.available_count > target_index && source.available_count > source_index); MoveConstruct(el, Get(el, target), target_index, Get(el, source), source_index); }

		static void Destruct(Element const& el, void* target) { el.id.wrapper_function(ArchetypeID::Status::Destruction, target, nullptr); }
		static void Destruct(Element const& el, RawArray const& target, std::size_t target_index) { Destruct(el, Get(target, target_index)); }
		static void Destruct(Element const& el, ArrayMountPoint const& target, std::size_t target_index) { Destruct(el, Get(el, target), target_index); }
		static void Destruct(Element const& el, MountPoint const& target) { Destruct(el, target.array_mp, target.array_mp_index); }

		void MoveConstruct(ArrayMountPoint const& target, std::size_t target_index, ArrayMountPoint const& source, std::size_t source_index) const
		{
			for(auto& ite : *this)
			{
				MoveConstruct(ite, target, target_index, source, source_index);
			}
		}

		void Destruct(ArrayMountPoint const& target, std::size_t target_index) const
		{
			for (auto& ite : *this)
			{
				Destruct(ite, target, target_index);
			}
		}

		Element const& GetInfos(std::size_t index) const { assert(index < infos.size()); return infos[index]; }
		Element const& operator[](std::size_t index) const { return GetInfos(index); }

		Element const* begin() const { return infos.data(); }
		Element const* end() const { return infos.data() + infos.size(); }

	protected:

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
	};

}