module;

export module NoodlesSystem;

import std;
import PotatoMisc;
import PotatoPointer;
import PotatoTaskSystem;
import PotatoIR;

import NoodlesArcheType;

export namespace Noodles
{
	extern struct ExecuteContext;
}

export namespace Noodles::System
{

	struct RWInfo
	{
		enum class RWProperty
		{
			Read,
			Write
		};

		RWProperty rw_property;
		Potato::IR::TypeID type_id;

		template<typename Type>
		static RWInfo GetComponent()
		{
			return RWInfo{
				std::is_const_v<Type>
				? RWProperty::Read : RWProperty::Write,
				Potato::IR::TypeID::CreateTypeID<std::remove_cvref_t<Type>>()
			};
		}
	};

	struct Property;

	struct Priority
	{
		std::int32_t layer = 0;
		std::int32_t primary_priority = 0;
		std::int32_t second_priority = 0;
		std::partial_ordering (*compare)(Property const& self, Property const& target) = nullptr;

		std::strong_ordering ComparePriority(Priority const& p2) const;
		std::partial_ordering CompareCustomPriority(Property const& self_property, Priority const& target, Property const& target_property) const;
	};

	struct Property
	{
		std::u8string_view system_name;
		std::u8string_view group_name;
		std::size_t task_priority = *Potato::Task::TaskPriority::Normal;
		
		bool IsSameSystem(Property const& oi) const
		{
			return group_name == oi.group_name && system_name == oi.system_name;
		}
	};

	struct Object
	{
		struct Ref
		{
			void (*func)(void* object, ExecuteContext& status) = nullptr;
			void* object = nullptr;

			operator bool() const { return func != nullptr; }
			void Execute(ExecuteContext& status) { func(object, status); }

			Ref(
				void (*func)(void* object, ExecuteContext& status) = nullptr,
				void* object = nullptr
			) : func(func), object(object) {}

			Ref(Ref const&) = default;
			Ref& operator=(Ref const&) = default;
		};

		Ref object;
		void (*destructor)(void*) = nullptr;

		Object(
			void (*func)(void*, ExecuteContext&),
			void* object = nullptr,
			void (*destructor)(void*) = nullptr
		);

		Object() = default;

		Object(Object&& obj);
		~Object();

		Object& operator=(Object&& obj);

		operator bool() const { return object; }
	};

	struct MutexProperty
	{
		std::span<RWInfo> component_rw_infos;

		bool IsConflig(MutexProperty const& p2) const;
	};

}