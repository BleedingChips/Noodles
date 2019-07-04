#pragma once
#include "../Interface/interface.h"
#include "component_pool.h"
#include "gobal_component_pool.h"
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

		virtual void* find_system(const TypeInfo& ti) noexcept override;
		virtual void regedit_system(SystemInterface*) noexcept override;
		virtual void destory_system(const TypeInfo&) noexcept override;
		virtual void regedit_template_system(SystemInterface*) noexcept override;

		bool update(bool component_change, bool gobal_component_change, ComponentPool& cp, GobalComponentPool& gcp);
		void clean_all();
		ApplyResult asynchro_apply_system(Context*, bool wait_for_lock = true);

	private:

		struct SystemHolder;
		using HoldType = std::map<TypeInfo, SystemHolder>;
		bool release_system(const TypeInfo& id);
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
		};
		HoldType m_systems;
		struct SystemRelationShip
		{
			HoldType::iterator active;
			HoldType::iterator passtive;
			bool is_mutex;
			std::vector<TypeInfo> conflig_type;
			std::array<size_t, 3> conflig_bound;
		};
		std::vector<SystemRelationShip> m_start_system;

		std::mutex m_state_mutex;
		std::vector<State> m_state;
		std::vector<HoldType::iterator> m_waitting_list;

		std::mutex m_log_mutex;
		std::vector<std::variant<SystemInterfacePtr, TypeInfo>> m_regedited_system;

		std::mutex m_template_mutex;
		std::deque<SystemInterfacePtr> m_template_system;
	};
}