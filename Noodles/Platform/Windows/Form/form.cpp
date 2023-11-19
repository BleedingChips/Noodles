module;

#include <cassert>
#include <Windows.h>

module NoodlesForm;

namespace Noodles::Form
{
	FormManager::FormManager(Potato::Task::TaskContext::Ptr context, GlobalFormSetting const& setting, std::pmr::memory_resource* resource)
		: context(std::move(context)), setting(setting), resource(resource)
	{
		assert(context);
	}

	auto FormManager::Create(Potato::Task::TaskContext::Ptr context, GlobalFormSetting const& setting, std::pmr::memory_resource* resource)
		-> Ptr
	{
		if(resource != nullptr && context)
		{
			auto adre = resource->allocate(sizeof(FormManager), alignof(FormManager));
			if(adre != nullptr)
			{
				Ptr ptr = new (adre) FormManager{
					std::move(context), setting, resource
				};
				assert(ptr);
				ptr->Fire();
				return ptr;
			}
		}
		return {};
	}

	void FormManager::Fire()
	{
		assert(context);
		Potato::Task::TaskProperty pro;
		pro.TaskPriority = setting.priority;
		pro.TaskName = setting.display_name;
		context->CommitTask(this, pro);
	}

	void FormManager::Release() override
	{
		auto ores = resource;
		this->~FormManager();
		ores->deallocate(this, sizeof(FormManager), alignof(FormManager));
	}

	void FormManager::operator()(Potato::Task::ExecuteStatus& status)
	{
		if(available)
		{
			bool has_message = false;
			MSG msg;
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				has_message = true;
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
			if(has_message)
			{
				context->CommitDelayTask(this, setting.update_rate, status.Property);
			}else
			{
				context->CommitDelayTask(this, setting.idle_update_rate, status.Property);
			}
		}
	}

	void FormManager::ControlRelease()
	{
		available = false;
	}

}