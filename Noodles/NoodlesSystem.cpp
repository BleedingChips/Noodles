module;
#
module NoodlesSystem;

namespace Noodles::System
{

	std::strong_ordering Priority::ComparePriority(Priority const& p2) const
	{
		auto r1 = layer <=> p2.layer;
		if(r1 == std::strong_ordering::equal)
		{
			r1 = primary_priority <=> p2.primary_priority;
			if (r1 == std::strong_ordering::equal)
			{
				return 	second_priority <=> p2.second_priority;
			}
		}
		return r1;
	}

	std::partial_ordering Reversal(std::partial_ordering r)
	{
		if(r == std::partial_ordering::greater)
			return std::partial_ordering::less;
		else if(r == std::partial_ordering::less)
			return std::partial_ordering::greater;
		else
			return r;
	}

	std::partial_ordering Priority::CompareCustomPriority(Property const& self_property, Priority const& target, Property const& target_property) const
	{
		auto r0 = ComparePriority(target);
		if(r0 != std::strong_ordering::equal)
		{
			return static_cast<std::partial_ordering>(r0);
		}

		std::partial_ordering r1 = std::partial_ordering::unordered;
		std::partial_ordering r2 = std::partial_ordering::unordered;

		if(compare != nullptr)
		{
			r1 = (*compare)(self_property, target_property);
		}

		if(target.compare != nullptr)
		{
			r2 = (*target.compare)(target_property, self_property);
		}

		if(r1 == std::partial_ordering::unordered)
			return Reversal(r2);
		else if(r1 == std::partial_ordering::equivalent)
		{
			if (r2 != std::partial_ordering::unordered)
				return Reversal(r2);
			else
				return std::partial_ordering::equivalent;
		}else
		{
			if (r1 != r2)
				return r1;
			else
				return std::partial_ordering::unordered;
		}
	}

	Object::Object(
		void (*func)(void*, ExecuteContext&),
		void* object,
		void (*destructor)(void*)
	) : object(func, object), destructor(destructor)
	{

	}

	Object::Object(Object&& obj)
		: object(obj.object), destructor(obj.destructor)
	{
		obj.destructor = nullptr;
		obj.object = {};
	}

	Object::~Object()
	{
		if (destructor != nullptr)
		{
			(*destructor)(object.object);
		}
	}

	Object& Object::operator=(Object&& obj)
	{
		this->~Object();
		new (this) Object{ std::move(obj) };
		return *this;
	}

	bool MutexProperty::IsConflig(MutexProperty const& p2) const
	{
		for(auto& ite : component_rw_infos)
		{
			bool is_write = (ite.rw_property == RWInfo::RWProperty::Write);
			for(auto& ite2 : p2.component_rw_infos)
			{
				if (is_write || ite2.rw_property == RWInfo::RWProperty::Write)
				{
					if ((ite.type_id <=> ite2.type_id) == std::strong_ordering::equal)
					{
						return true;
					}
				}
			}
		}
		return false;
	}
}