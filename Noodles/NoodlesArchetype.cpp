module;

#include <cassert>

module NoodlesArchetype;

namespace Noodles
{
	std::strong_ordering ArchetypeID::operator<=>(ArchetypeID const& i2) const
	{
		return Potato::Misc::PriorityCompareStrongOrdering(
			layout.Align, i2.layout.Align,
			layout.Size, i2.layout.Size,
			i2.id, id
		);
	}

	std::strong_ordering ArchetypeMountPoint::operator<=>(ArchetypeMountPoint const& mp) const
	{
		auto re = index <=> mp.index;
		if(re == std::strong_ordering::equivalent)
		{
			re = buffer <=> mp.buffer;
		}
		return re;
	}

	ArchetypeMountPoint::operator bool() const
	{
		return buffer != nullptr && element_count >= 1 && index < element_count;
	}

	bool Archetype::CheckAcceptable(std::span<ArchetypeID const> id1, std::span<ArchetypeID const> id2)
	{
		for(std::size_t i = 0; i < id1.size(); ++i)
		{
			auto& ite = id1[i];
			if(ite.is_singleton)
			{
				for(std::size_t i2 = i + 1; i2 < id1.size(); ++i2)
				{
					auto& ite2 = id1[i2];
					if(ite2.is_singleton)
					{
						if(ite2.id == ite.id)
							return false;
					}
				}

				for(std::size_t i2 = 0; i2 < id2.size(); ++i2)
				{
					auto& ite2 = id2[i2];
					if (ite2.is_singleton)
					{
						if (ite2.id == ite.id)
							return false;
					}
				}
			}
		}

		for (std::size_t i = 0; i < id2.size(); ++i)
		{
			auto& ite = id1[i];
			if (ite.is_singleton)
			{
				for (std::size_t i2 = i + 1; i2 < id1.size(); ++i2)
				{
					auto& ite2 = id1[i2];
					if (ite2.is_singleton)
					{
						if (ite2.id == ite.id)
							return false;
					}
				}
			}
		}

		return id1.size() + id2.size();
	}

	auto Archetype::Create(std::span<ArchetypeID const> ref_info, std::span<ArchetypeID const> append_info, std::pmr::memory_resource* resource)
	->Ptr
	{
		assert(resource != nullptr);

		auto re = CheckAcceptable(ref_info, append_info);

		if(!re)
		{
			return {};
		}

		auto info_count = ref_info.size() + append_info.size();
		auto info_size = info_count * sizeof(Element);
		auto total_size = info_size + sizeof(Archetype);
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
			for(auto& ite : append_info)
			{
				new (ite_adress) Element{ ite };
				ite_adress += 1;
			}
			std::span<Element> infos{ temp_adress, info_count };
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
				ite.offset = Potato::IR::InsertLayoutCPP(layout, ite.id.layout);
			}

			ptr->buffer_archetype_layout_size = layout.Size;

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

	Archetype::~Archetype()
	{
		for (auto& ite : infos)
		{
			ite.~Element();
		}
		infos = {};
	}

	void Archetype::Release()
	{
		auto old_resource = resource;
		auto old_size = allocated_size;
		this->~Archetype();
		old_resource->deallocate(
			this,
			old_size,
			alignof(Archetype)
		);
	}

	std::optional<std::size_t> Archetype::LocateTypeID(UniqueTypeID const& type_id, std::size_t require_index) const
	{
		std::size_t index = 0;
		for(auto& ite : infos)
		{
			auto re = type_id <=> ite.id.id;
			if (re == std::strong_ordering::equal)
			{
				if(require_index == 0)
					return index;
				else
				{
					require_index -= 1;
				}
			}
			else if (re == std::strong_ordering::less)
			{
				break;
			}
			++index;
		}
		return std::nullopt;
	}

	UniqueTypeID const& Archetype::GetTypeID(std::size_t index) const
	{
		assert(GetTypeIDCount() > index);
		return infos[index].id.id;
	}

	void* Archetype::GetData(std::size_t locate_index, ArchetypeMountPoint mount_point) const
	{
		assert(GetTypeIDCount() > locate_index);
		assert(mount_point);
		auto& ref = infos[locate_index];
		auto layout = ref.id.layout;
		auto offset = ref.offset;
		return static_cast<void*>(static_cast<std::byte*>(mount_point.buffer) + offset * mount_point.element_count + layout.Size * mount_point.index);
	}

	void Archetype::MoveConstruct(std::size_t locate_index, void* target, void* source) const
	{
		assert(GetTypeIDCount() > locate_index);
		assert(target != nullptr && source != nullptr);
		auto& ref = infos[locate_index];
		ref.id.wrapper_function(ArchetypeID::Status::MoveConstruction, target, source);
	}

	void Archetype::Destruction(std::size_t locate_index, void* target) const
	{
		assert(GetTypeIDCount() > locate_index);
		assert(target != nullptr);
		auto& ref = infos[locate_index];
		ref.id.wrapper_function(ArchetypeID::Status::Destruction, target, nullptr);
	}

	void Archetype::DefaultConstruct(std::size_t locate_index, void* target) const
	{
		assert(GetTypeIDCount() > locate_index);
		assert(target != nullptr);
		auto& ref = infos[locate_index];
		ref.id.wrapper_function(ArchetypeID::Status::DefaultConstruction, target, nullptr);
	}

	Archetype::Ptr Archetype::Clone(std::pmr::memory_resource* o_resource) const
	{
		if(o_resource != nullptr)
		{
			auto new_adress = o_resource->allocate(allocated_size, alignof(Archetype));
			if(new_adress != nullptr)
			{
				Ptr ptr = new (new_adress) Archetype{ o_resource, allocated_size };
				assert(ptr);
				
				std::span<Element> new_infos{
					reinterpret_cast<Element*>(ptr.GetPointer() + 1),
					infos.size()
				};
				auto ite2 = new_infos;
				for(auto& ite : infos)
				{
					new (&ite2[0]) Element{ite};
					ite2 = ite2.subspan(1);
				}
				ptr->archetype_layout = archetype_layout;
				ptr->infos = new_infos;
				ptr->buffer_archetype_layout_size = buffer_archetype_layout_size;
				return ptr;
			}
		}
		return {};
	}

	std::strong_ordering Archetype::operator<=>(Archetype const& i2) const
	{
		auto re = infos.size() <=> i2.infos.size();
		if(re == std::strong_ordering::equivalent)
		{
			for(std::size_t i = 0; i < infos.size(); ++i)
			{
				auto& i1 = infos[i];
				auto& i2 = infos[i];
				re = (i1.id <=> i2.id);
				if(re != std::strong_ordering::equivalent)
					return re;
			}
		}
		return re;
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