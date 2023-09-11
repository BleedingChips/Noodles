module;

export module NoodlesArcheType;

import std;
import PotatoIR;
import PotatoPointer;

export namespace Noodles
{

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
				[](void* Source, void* Target){ new (Target) Type{ static_cast<Type&&>(*Source) }; },
				[](void* Target) { static_cast<Type*>(Target)->~Type(); }
			};
		}
	};

	struct ArcheType : public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<ArcheType>;
		bool HasType(Potato::IR::TypeID ID) const;
		void* LocateType(Potato::IR::TypeID ID) const;
		void* Copy(Potato::IR::TypeID RequireID) const;
		virtual std::size_t HashID() const = 0;
		static Ptr Create(std::span<TypeInfo const> InputSpan, std::pmr::memory_resource* IResource);

	protected:

		virtual void Release() override;

		std::pmr::memory_resource* Resource;
		std::span<TypeInfo const> TypeInfos;
	};





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