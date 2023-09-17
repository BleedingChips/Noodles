module;

module NoodlesArcheType;

namespace Noodles
{

	


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