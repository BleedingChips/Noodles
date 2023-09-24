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
	struct SystemRWInfo
	{
		enum class Category
		{
			Component,
			GobalComponent,
		};

		enum class Property
		{
			Read,
			Write
		};

		Category category;
		Potato::IR::TypeID type_id;
		Property rw_property;
	};

	struct SystemProperty
	{
		enum class Category
		{
			Init,
			PreDestroy,
			Tick
		};

		Category category = Category::Tick;
		std::size_t task_priority = *Potato::Task::TaskPriority::Normal;
		std::int64_t layer = 0;
		std::int64_t system_priority = 0;
		std::u8string_view group_name;
		std::u8string_view system_name = u8"UnName System";
	};

	struct ExecuteContext;

	struct SystemObject
	{
		void (*func)(void* object, ExecuteContext& status) = nullptr;
		void* object = nullptr;
		void (*destructor)(void* object) = nullptr;
		std::weak_ordering (*compare_func)(void* Object, SystemProperty const& SelfProperty, SystemProperty const& target_property) = nullptr;
	};

}