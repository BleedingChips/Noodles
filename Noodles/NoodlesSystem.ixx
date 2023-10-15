module;

export module NoodlesSystem;

import std;
import PotatoTMP;
import PotatoMisc;
import PotatoPointer;
import PotatoTaskSystem;
import PotatoIR;

import NoodlesArcheType;
import NoodlesComponent;

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

	struct MutexProperty
	{
		std::span<RWInfo> component_rw_infos;
		std::span<RWInfo> global_component_rw_infos;

		bool IsConflict(MutexProperty const& p2) const;
	};

}

export namespace Noodles
{
	struct Context;

	struct ExecuteContext
	{
		System::Property property;
		Context& context;
	};
}

namespace Noodles::System
{

	template<typename Func>
	concept AcceptableSystemObject = true;

	export enum class RunningStatus
	{
		PreInit,
		Waiting,

		Ready,
		Running,
		Done,
	};

	export struct RunningContext
	{
		RunningStatus status;
		std::size_t startup_in_degree = 0;
		std::size_t current_in_degree = 0;
		std::size_t mutex_degree = 0;

		Property property;
		Potato::Misc::IndexSpan<> reference_trigger_line;
		
		
		virtual void Execute(ExecuteContext& context) = 0;
		virtual void Release(std::pmr::memory_resource* resource) = 0;
	};

	export struct TriggerLine
	{
		bool is_mutex = false;
		RunningContext* target = nullptr;
	};

	export template<typename Func>
	struct RunningContextCallableObject : public RunningContext
	{
		std::conditional_t<
			std::is_function_v<Func>,
			Func*,
			Func
		> fun;
		RunningContextCallableObject(Func&& fun, std::pmr::memory_resource* resource)
			: fun(std::move(fun))
		{

		}
		void Execute(ExecuteContext& con) override
		{
			fun(con);
		}
		void Release(std::pmr::memory_resource* resource)
		{
			this->~RunningContextCallableObject();
			resource->deallocate(
				this,
				sizeof(RunningContextCallableObject),
				alignof(RunningContextCallableObject)
			);
		}
	};

	template<typename FuncT>
	concept HasCertainlyOperatorParentheses = requires(FuncT fun)
	{
		{&FuncT::operator()};
	};

	template<typename FuncT>
	struct CallableObjectParameter
	{
		using Type = Potato::TMP::FunctionInfo<decltype(&FuncT::operator())>;
	};

	template<typename FuncT>
	struct FunctionPointerParameter
	{
		using Type = Potato::TMP::FunctionInfo<FuncT>;
	};

	template<typename FuncT>
	using ExtractFunctionParameterTypeT = typename std::conditional_t<
		std::is_function_v<FuncT>,
		Potato::TMP::Instant<FunctionPointerParameter>,
		Potato::TMP::Instant<CallableObjectParameter>
	>:: template AppendT<FuncT>;


	export template<typename Func>
		RunningContext* CreateObjFromCallableObject(Func&& func, std::pmr::memory_resource* resource) requires(
			std::is_function_v<std::remove_cvref_t<Func>> || HasCertainlyOperatorParentheses<std::remove_cvref_t<Func>>
		)
	{
		using ExtractType = ExtractFunctionParameterTypeT<std::remove_cvref_t<Func>>;
		int o = ExtractType{};
		if (resource != nullptr)
		{
			using OT = RunningContextCallableObject<std::remove_cvref_t<Func>>;
			auto adress = resource->allocate(sizeof(OT), alignof(OT));
			if (adress != nullptr)
			{
				return new (adress) OT{ std::forward<Func>(func), resource };
			}
		}
		return {};
	}


	/*
	template<bool IsFunctionPointer, typename FuncT>
	struct FunctionTypeFilter
	{
		using T = Potato::TMP::
	};

	export template<typename FuncT>
	MutexProperty const& GetMutexPropertyFromFunction()
	{
		static_assert();
	}*/

}
