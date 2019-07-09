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
		std::vector<const char*> conflig_type;
		std::array<size_t, 3> conflig_bound;
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

		enum class RuningState
		{
			Ready,
			Using,
			Done
		};

		struct SystemHolder
		{
			SystemInterfacePtr ptr;
			size_t state_index;
			size_t relationship_start_index;
			size_t relationship_length;
		};

		using SystemHoldMap = std::map<TypeInfo, SystemHolder>;

		struct SystemRelationShip
		{
			SystemHoldMap::iterator active;
			SystemHoldMap::iterator passtive;
			bool is_force = false;
			bool is_mutex = true;
			std::vector<TypeInfo> conflig_type;
			std::array<size_t, 3> conflig_bound;
			size_t component_check_start = 0;
			size_t component_check_length = 0;
		};

		struct SystemState
		{
			RuningState state = RuningState::Ready;
			size_t relationship_start_index = 0;
			size_t relationship_length = 0;
			SystemHoldMap::iterator pointer;
		};

		struct SystemRunningRelationShip
		{
			size_t passtive_index;
			bool is_force = false;
			bool is_mutex = true;
			bool gobal_component_check = true;
			bool system_check = true;
			bool component_check = true;
		};

		std::shared_mutex m_systems_mutex;
		SystemHoldMap m_systems;
		std::vector<SystemRelationShip> m_relationships;
		bool m_systems_change = false;

		std::mutex m_state_mutex;
		std::vector<SystemState> m_state;
		std::vector<SystemRunningRelationShip> m_running_relationship;
		std::vector<size_t> m_conflig_component_state;
		std::vector<bool> m_component_state;
		bool m_all_done = true;

		std::mutex m_log_mutex;
		std::vector<std::variant<SystemInterfacePtr, TypeInfo>> m_regedited_system;

		bool release_system(const TypeInfo& ite);
		std::optional<std::tuple<TickOrder, bool, std::vector<TypeInfo>, std::array<size_t, 3>>> handle_system_conflig(SystemHoldMap::iterator, SystemHoldMap::iterator);
		void handle_relationship_component_conflig(SystemRelationShip&, size_t component_size);
	};
}