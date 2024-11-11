module;

#include <cassert>
export module NoodlesArchetype;

import std;
import PotatoIR;
import PotatoPointer;
import PotatoTaskSystem;

export namespace Noodles
{

	using AtomicType = Potato::IR::StructLayout;

	template<typename Type>
	inline AtomicType::Ptr GetAtomicType() { return Potato::IR::StaticAtomicStructLayout<std::remove_cvref_t<Type>>::Create(); }

	struct Archetype : protected Potato::IR::StructLayout, public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<Archetype>;
		using OPtr = Potato::Pointer::ObserverPtr<Archetype const>;

		static std::optional<std::size_t> CalculateHashCode(std::span<AtomicType::Ptr> atomic_type);
		static Ptr Create(std::span<AtomicType::Ptr> atomic_type, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		std::optional<std::size_t> LocateTypeID(AtomicType const& type_id) const;
		std::size_t GetTypeIDCount() const { return GetMemberView().size(); }
		AtomicType::Ptr GetTypeID(std::size_t index) const;
		using StructLayout::GetHashCode;

		Potato::IR::Layout GetLayout() const override { return single_layout; }
		Potato::IR::Layout GetArchetypeLayout() const { return archetype_layout; }
		std::strong_ordering operator<=>(Archetype const& il) const { return StructLayout::operator<=>(il); }
		std::pmr::memory_resource* GetResource() const { return record.GetMemoryResource(); }

		virtual std::span<MemberView const> GetMemberView() const override { return member_view; }
		virtual std::size_t GetHashCode() const override { return hash_code; }

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
		static RawArray Get(MemberView const& ref, ArrayMountPoint mount_point);
		static void* Get(MemberView const& ref, ArrayMountPoint mount_point, std::size_t array_index) { return  Get(Get(ref,mount_point), array_index); }
		static void* Get(MemberView const& ref, MountPoint mount_point) { return  Get(ref, mount_point.array_mp, mount_point.array_mp_index); }

		RawArray Get(std::size_t index, ArrayMountPoint mount_point) const { return Get(this->operator[](index), mount_point); }
		void* Get(std::size_t index, ArrayMountPoint mount_point, std::size_t mount_point_index) const { return Get(this->operator[](index), mount_point, mount_point_index); }


		static void MoveConstruct(MemberView const& el, void* target, void* source) { el.struct_layout->MoveConstruction(target, source); }
		static void MoveConstruct(MemberView const& el, RawArray const& target, std::size_t target_index, RawArray const& source, std::size_t source_index) { MoveConstruct(el, Get(target, target_index), Get(source, source_index)); }
		static void MoveConstruct(MemberView const& el, ArrayMountPoint const& target, std::size_t target_index, ArrayMountPoint const& source, std::size_t source_index) { assert(target.available_count > target_index && source.available_count > source_index); MoveConstruct(el, Get(el, target), target_index, Get(el, source), source_index); }

		static void Destruct(MemberView const& el, void* target) { el.struct_layout->Destruction(target); }
		static void Destruct(MemberView const& el, RawArray const& target, std::size_t target_index) { Destruct(el, Get(target, target_index)); }
		static void Destruct(MemberView const& el, ArrayMountPoint const& target, std::size_t target_index) { Destruct(el, Get(el, target), target_index); }
		static void Destruct(MemberView const& el, MountPoint const& target) { Destruct(el, target.array_mp, target.array_mp_index); }

		void MoveConstruct(ArrayMountPoint const& target, std::size_t target_index, ArrayMountPoint const& source, std::size_t source_index) const
		{
			for(auto& ite : member_view)
			{
				MoveConstruct(ite, target, target_index, source, source_index);
			}
		}

		void Destruct(ArrayMountPoint const& target, std::size_t target_index) const
		{
			for (auto& ite : member_view)
			{
				Destruct(ite, target, target_index);
			}
		}

		decltype(auto) begin() const { return member_view.begin(); }
		decltype(auto) end() const { return member_view.end(); }
		MemberView const& operator[](std::size_t index) const { return member_view[index]; }

	protected:

		Archetype(
			Potato::IR::MemoryResourceRecord record,
			Potato::IR::Layout single_layout,
			Potato::IR::Layout archetype_layout,
			std::span<MemberView const> member_view,
			std::size_t hash_code,
			OperateProperty ope_property
			)
				: record(record), single_layout(single_layout), archetype_layout(archetype_layout), member_view(member_view), hash_code(hash_code), ope_property(ope_property)
		{
			
		}


		~Archetype() = default;

		virtual void AddStructLayoutRef() const override { DefaultIntrusiveInterface::AddRef(); }
		virtual void SubStructLayoutRef() const override { DefaultIntrusiveInterface::SubRef(); }
		virtual std::u8string_view GetName() const override {return {};}
		virtual OperateProperty GetOperateProperty() const override { return ope_property; }
		virtual void Release() override;

		Potato::IR::MemoryResourceRecord record;
		Potato::IR::Layout single_layout;
		Potato::IR::Layout archetype_layout;
		std::span<MemberView const> member_view;
		std::size_t hash_code;
		OperateProperty ope_property;
	};

}