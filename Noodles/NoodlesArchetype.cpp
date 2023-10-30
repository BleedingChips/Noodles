module;

#include <cassert>

module NoodlesArchetype;

namespace Noodles
{
	std::strong_ordering operator<=>(UniqueTypeID const& i1, UniqueTypeID const& i2)
	{
		return Potato::Misc::PriorityCompareStrongOrdering(
			i1.layout.Align, i2.layout.Align,
			i1.layout.Size, i2.layout.Size,
			i1.id, i2.id
		);
	}

	std::strong_ordering operator<=>(ArchetypeID const& i1, ArchetypeID const& i2);



	auto Archetype::Create(std::span<ArchetypeID const> ref_info, std::pmr::memory_resource* resource)
	->Ptr
	{
		assert(resource != nullptr);
		auto info_count = ref_info.size() * sizeof(Element);
		auto total_size = info_count + sizeof(Archetype);
		auto adress = resource->allocate(total_size, alignof(Archetype));
		if(adress != nullptr)
		{
			Ptr ptr = new (adress) Archetype{ resource, total_size };
			assert(ptr);
			Element* temp_adress = reinterpret_cast<Element*>(ptr.GetPointer() + 1);
			auto ite_adress = temp_adress;
			for(auto& ite : ref_info)
			{
				new (ite_adress) Element{ite};
				ite_adress += 1;
			}
			std::span<Element> infos{ temp_adress, ref_info.size() };
			std::sort(infos.begin(), infos.end(), [](Element const& E, Element const& E2)
			{
				auto re = E.id <=> E2.id;
				if(re != std::strong_ordering::less)
					return true;
				return false;
			});

			Potato::IR::Layout layout;

			for(auto& ite : infos)
			{
				ite.offset = Potato::IR::InsertLayoutCPP(layout, ite.id.id.layout);
			}

			Potato::IR::FixLayoutCPP(layout);

			ptr->infos = infos;
			ptr->archetype_layout = layout;

			return ptr;
		}
		return {};
	}
	
	Archetype::Archetype(std::pmr::memory_resource* resouce, std::size_t allocated_size)
		: resource(resouce), allocated_size(allocated_size)
	{
		
	}

	void Archetype::Release()
	{
		auto old_resource = resource;
		auto old_size = allocated_size;
		for(auto& ite : infos)
		{
			ite.~Element();
		}
		this->~Archetype();
		old_resource->deallocate(
			this,
			old_size,
			alignof(Archetype)
		);
	}

	auto Archetype::LocateTypeImplementation(UniqueTypeID unique_id) const
	->Element const*
	{
		for (auto& ite : infos)
		{
			auto re = unique_id <=> ite.id.id;
			if (re == std::strong_ordering::equal)
			{
				return &ite;
			}
			else if (re == std::strong_ordering::greater)
			{
				break;
			}
		}
		return nullptr;
	}

	void* Archetype::LocateType(UniqueTypeID unique_id, void* buffer, std::size_t index, std::size_t total_index_count) const
	{
		assert(buffer != nullptr);
		auto re  = LocateTypeImplementation(unique_id);
		if(re != nullptr)
		{
			auto [layout, offset] = *re;
			return static_cast<void*>(static_cast<std::byte*>(buffer) + offset * total_index_count + layout.id.layout.Size * index);
		}
		return nullptr;
	}



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