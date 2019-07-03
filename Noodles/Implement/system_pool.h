#pragma once
#include "interface.h"
#include <map>
#include <shared_mutex>
#include <set>
#include <tuple>
#include <vector>
#include <variant>
#include <deque>

namespace Noodles::Error
{
	struct SystemOrderRecursion
	{
		std::vector<const char*> infos;
	};

	struct SystemOrderConflig
	{
		const char* si;
		const char* ti;
	};
}


namespace Noodles::Implement
{
	struct SystemPool : SystemPoolInterface
	{

		enum class ApplyResult
		{
			Waitting,
			Applied,
			AllDone,
		};

		virtual void regedit_system(SystemInterface*) noexcept override;
		virtual void destory_system(const TypeLayout&) noexcept override;
		virtual void regedit_template_system(TemplateSystemInterface*) noexcept override;

		bool update();
		void clean_all();
		ApplyResult asynchro_apply_system(Context*, bool wait_for_lock = true);
		void synchro_apply_template_system(Context*);

	private:

		struct SystemHolder;
		using HoldType = std::map<TypeLayout, SystemHolder>;
		bool release_system(const TypeLayout& id);
		virtual SystemInterface* find_system(const TypeLayout& ti) noexcept override;
		static void update_new_system_order(HoldType::iterator, HoldType::iterator);
		static void set_system_order(HoldType::iterator, HoldType::iterator, TickOrder order);


		enum State
		{
			Ready,
			Using,
			Done
		};

		std::shared_mutex m_system_mutex;
		struct SystemHolder
		{
			SystemInterfacePtr ptr;
			size_t state_index;
			std::vector<std::tuple<bool, size_t>> mutex_and_dependence;
			std::vector<typename HoldType::iterator> derived;
		};
		HoldType m_systems;
		std::vector<HoldType::iterator> m_start_system;

		std::mutex m_state_mutex;
		std::vector<State> m_state;
		std::vector<HoldType::iterator> m_waitting_list;

		std::mutex m_log_mutex;
		std::vector<std::variant<SystemInterfacePtr, TypeLayout>> m_regedited_system;

		std::mutex m_template_mutex;
		std::deque<TemplateSystemInterfacePtr> m_template_system;
	};
}