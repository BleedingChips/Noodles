module;
#
module NoodlesSystem;

namespace Noodles
{

	std::strong_ordering SystemPriority::ComparePriority(SystemPriority const& p2) const
	{
		return Potato::Misc::PriorityCompareStrongOrdering(
			primary_priority, p2.primary_priority,
			second_priority, p2.second_priority
		);
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

	std::partial_ordering SystemPriority::CompareCustomPriority(SystemProperty const& self_property, SystemPriority const& target, SystemProperty const& target_property) const
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

	bool DetectConflict(std::span<SystemRWInfo const> t1, std::span<SystemRWInfo const> t2)
	{
		auto ite1 = t1.begin();
		auto ite2 = t2.begin();

		while(ite1 != t1.end() && ite2 != t2.end())
		{
			auto re = ite1->type_id <=> ite2->type_id;
			if(re == std::strong_ordering::equal)
			{
				if(
					ite1->is_write
					|| ite2->is_write
					)
				{
					return true;
				}else
				{
					++ite2; ++ite1;
				}
			}else if(re == std::strong_ordering::less)
			{
				++ite1;	
			}else
			{
				++ite2;
			}
		}
		return false;
	}

	bool SystemMutex::IsConflict(SystemMutex const& p2) const
	{
		auto re1 = DetectConflict(component_rw_infos, p2.component_rw_infos);
		return re1;
	}

	FilterGenerator::FilterGenerator(std::pmr::memory_resource* ptr)
		: resource(ptr), component_rw_info(ptr), global_component_rw_info(ptr)
	{
		
	}

	void OrderedInsert(SystemRWInfo const& tar, std::pmr::vector<SystemRWInfo>& vec)
	{
		auto find = std::find_if(
			vec.begin(),
			vec.end(),
			[&](SystemRWInfo const& o)
			{
				return tar.type_id <= o.type_id;
			}
		);
		if (find == vec.end() || find->type_id == tar.type_id)
		{
			if(tar.is_write)
			{
				find->is_write = true;
			}
		}else
		{
			vec.insert(
				find,
				tar
			);
		}
	}

	void FilterGenerator::AddComponentFilter(std::span<SystemRWInfo> rw_infos)
	{

		std::pmr::vector<SystemRWInfo> infos{ resource };

		for(auto& ite : rw_infos)
		{
			OrderedInsert(
				ite,
				infos
			);
		}

		for(auto& ite : rw_infos)
		{
			OrderedInsert(
				ite,
				component_rw_info
			);
		}

		filter_element.emplace_back(
			Type::Component,
			std::move(infos)
		);
	}


	void FilterGenerator::AddGlobalComponentFilter(SystemRWInfo const& info)
	{
		std::pmr::vector<SystemRWInfo> infos{ resource };
		infos.push_back(info);
		OrderedInsert(
			info,
			global_component_rw_info
		);
	}

	void FilterGenerator::AddEntityFilter(std::span<SystemRWInfo> infos)
	{
		for(auto& ite : infos)
		{
			OrderedInsert(
				ite,
				global_component_rw_info
			);
		}
	}

	TickSystemsGroup::TickSystemsGroup(std::pmr::memory_resource* resource)
		: system_holder_resource(resource), graphic_node(resource), startup_system_context(resource), tick_systems_running_graphic_line(resource)
	{
		
	}

	/*
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
	*/

	
}