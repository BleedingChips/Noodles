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

	namespace Error
	{
		struct OutOfArchetypeIndex
		{
			std::size_t current;
		};
	}

	template<typename Type>
	inline AtomicType::Ptr GetAtomicType() { return Potato::IR::StaticAtomicStructLayout<std::remove_cvref_t<Type>>::Create(); }

	inline std::size_t StorageCalculate(std::size_t count)
	{
		auto i = count / 64;
		auto i2 = count % 64;
		if (i2 == 0)
		{
			return i;
		}
		else
		{
			return i + 1;
		}
	}

	struct AtomicTypeID
	{
		std::size_t index = 0;
		std::strong_ordering operator <=>(AtomicTypeID const&) const = default;
		bool operator ==(AtomicTypeID const&) const = default;
		static void Destruction(std::span<AtomicTypeID> index)
		{
			for (auto& ite : index)
			{
				ite.~AtomicTypeID();
			}
		}
	};

	struct AtomicTypeMark
	{
		std::uint64_t mark = 0;
		static std::optional<bool> Mark(std::span<AtomicTypeMark> marks, AtomicTypeID index, bool mark = true);
		static std::optional<bool> CheckIsMark(std::span<AtomicTypeMark const> marks, AtomicTypeID index);
		static void Destruction(std::span<AtomicTypeMark> marks)
		{
			for(auto ite : marks)
			{
				ite.~AtomicTypeMark();
			}
		}
		static bool Inclusion(std::span<AtomicTypeMark const> source, std::span<AtomicTypeMark const> target);
		static bool IsOverlapping(std::span<AtomicTypeMark const> source, std::span<AtomicTypeMark const> target);
		static bool Reset(std::span<AtomicTypeMark> target);
		static bool IsSame(std::span<AtomicTypeMark const> source, std::span<AtomicTypeMark const> target);
	};

	struct AtomicTypeManager
	{
		
		std::optional<AtomicTypeID> LocateOrAddAtomicType(AtomicType::Ptr const& type);
		AtomicTypeManager(std::size_t atomic_type_count = 128, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
			: storage_count(StorageCalculate(atomic_type_count)),  atomic_type(resource)
		{
		}
		std::size_t GetMaxAtomicTypeCount() const { return storage_count * 64; }
		std::size_t GetStorageCount() const { return storage_count; }
	protected:
		std::optional<AtomicTypeID> LocateAtomicType_AssumedLocked(AtomicType::Ptr const& type) const;
		std::size_t const storage_count = 0;
		std::shared_mutex mutex;
		std::pmr::vector<AtomicType::Ptr> atomic_type;
	};

	struct Archetype : protected Potato::IR::MemoryResourceRecordIntrusiveInterface
	{

		struct MemberView
		{
			Potato::IR::StructLayout::Ptr layout;
			AtomicTypeID atomic_type_id;
			std::size_t offset;
		};

		struct Index
		{
			std::size_t index = std::numeric_limits<std::size_t>::max();
			std::strong_ordering operator <=>(Index const&) const = default;
			bool operator ==(Index const&) const = default;
		};

		using Ptr = Potato::Pointer::IntrusivePtr<Archetype>;
		using OPtr = Potato::Pointer::ObserverPtr<Archetype const>;

		static std::optional<std::size_t> CalculateHashCode(std::span<AtomicType::Ptr> atomic_type);

		static Ptr Create(AtomicTypeManager& manager, std::span<AtomicType::Ptr const> atomic_type, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		std::optional<Archetype::Index> LocateAtomicTypeID(AtomicTypeID index) const;
		std::size_t GetAtomicTypeCount() const { return GetMemberView().size(); }
		AtomicType::Ptr GetTypeID(std::size_t index) const;

		Potato::IR::Layout GetLayout() const { return archetype_layout.Get(); }
		Potato::IR::Layout GetArchetypeLayout() { return archetype_layout.GetRawLayout(); }

		std::span<MemberView const> GetMemberView() const { return member_view; }
		MemberView const& GetMemberView(Archetype::Index index) const { return member_view[index.index]; }

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
				assert(element_layout.align == alignof(Type) && element_layout.size == sizeof(Type));
				return std::span(static_cast<Type*>(buffer), array_count);
			}
		};

		static void* Get(RawArray raw_data, std::size_t array_index);
		static RawArray Get(MemberView const& ref, ArrayMountPoint mount_point);
		static void* Get(MemberView const& ref, ArrayMountPoint mount_point, std::size_t array_index) { return  Get(Get(ref,mount_point), array_index); }
		static void* Get(MemberView const& ref, MountPoint mount_point) { return  Get(ref, mount_point.array_mp, mount_point.array_mp_index); }

		RawArray Get(std::size_t index, ArrayMountPoint mount_point) const { return Get(this->operator[](index), mount_point); }
		void* Get(std::size_t index, ArrayMountPoint mount_point, std::size_t mount_point_index) const { return Get(this->operator[](index), mount_point, mount_point_index); }


		static void MoveConstruct(MemberView const& el, void* target, void* source, std::size_t array_count = 1) { el.layout->MoveConstruction(target, source, array_count); }
		static void MoveConstruct(MemberView const& el, RawArray const& target, std::size_t target_index, RawArray const& source, std::size_t source_index) { MoveConstruct(el, Get(target, target_index), Get(source, source_index)); }
		static void MoveConstruct(MemberView const& el, ArrayMountPoint const& target, std::size_t target_index, ArrayMountPoint const& source, std::size_t source_index) { assert(target.available_count > target_index && source.available_count > source_index); MoveConstruct(el, Get(el, target), target_index, Get(el, source), source_index); }

		static void Destruct(MemberView const& el, void* target) { el.layout->Destruction(target); }
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
		std::span<AtomicTypeMark const> GetAtomicTypeMark() const { return archetype_mark; }

	protected:

		Archetype(
			Potato::IR::MemoryResourceRecord record,
			Potato::MemLayout::MemLayoutCPP archetype_layout,
			std::span<MemberView const> member_view,
			std::span<AtomicTypeMark> archetype_mark
			)
				: MemoryResourceRecordIntrusiveInterface(record),
			archetype_layout(archetype_layout),
			member_view(member_view),
			archetype_mark(archetype_mark)
		{
			
		}

		std::span<AtomicTypeMark> archetype_mark;

		~Archetype();

		Potato::MemLayout::MemLayoutCPP archetype_layout;
		std::span<MemberView const> member_view;

		friend struct Potato::Pointer::DefaultIntrusiveWrapper;
	};

}