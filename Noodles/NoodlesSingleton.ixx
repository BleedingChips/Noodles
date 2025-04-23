module;

#include <cassert>

export module NoodlesSingleton;

import std;
import Potato;
import NoodlesBitFlag;
import NoodlesClassBitFlag;
import NoodlesArchetype;

export namespace Noodles
{

	export struct SingletonManager
	{

		struct Config
		{
			std::pmr::memory_resource* singleton_resource = std::pmr::get_default_resource();
			std::pmr::memory_resource* resource = std::pmr::get_default_resource();
		};

		SingletonManager(std::size_t singleton_container_count, Config config = {});
		~SingletonManager();

		std::size_t GetSingletonVersion() const { return version; }
		static void ResetQueryData(std::span<std::size_t> target)
		{
			for (auto& ite : target) ite = std::numeric_limits<std::size_t>::max();
		}

		constexpr static std::size_t GetQueryDataCount() { return 1; }

		std::size_t TranslateBitFlagToQueryData(std::span<BitFlag const> bitflag, std::span<std::size_t> output) const;
		std::size_t QuerySingletonData(std::span<std::size_t> query_data, std::span<void*> output_singleton) const;
		BitFlagContainerConstViewer GetSingletonUpdateBitFlagViewer() const { return singleton_update_bitflag; }
		BitFlagContainerConstViewer GetSingletonUsageBitFlagViewer() const { return singleton_usage_bitflag; }

	protected:

		std::pmr::unsynchronized_pool_resource singleton_resource;
		Archetype::Ptr singleton_archetype;
		Potato::IR::MemoryResourceRecord singleton_record;
		
		std::size_t version = 0;

		BitFlagContainer bitflag;
		BitFlagContainerViewer singleton_usage_bitflag;
		BitFlagContainerViewer singleton_update_bitflag;

		friend struct SingletonModifyManager;
	};


	export struct SingletonModifyManager
	{
		SingletonModifyManager(std::size_t singleton_container_count, std::pmr::memory_resource* resource = std::pmr::get_default_resource());
		template<typename SingletonType>
		bool AddSingleton(SingletonType&& singleton, AsynClassBitFlagMap& map, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
		{
			return this->AddSingleton(std::forward<SingletonType>(singleton), *map.LocateOrAdd<SingletonType>(), resource);
		}

		template<typename SingletonType>
		bool AddSingleton(SingletonType&& singleton, BitFlag singleton_bitflag, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
		{
			return AddSingleton(*Potato::IR::StaticAtomicStructLayout<SingletonType>::Create(), singleton_bitflag, std::is_rvalue_reference_v<SingletonType&&> ? true : false, &singleton, resource);
		}

		bool AddSingleton(StructLayout const& singleton_class, BitFlag singleton_bitflag, bool is_move_construct, void* singleton_data, std::pmr::memory_resource* resource = std::pmr::get_default_resource());

		bool RemoveSingleton(BitFlag singleton_bitflag);
		bool FlushSingletonModify(SingletonManager& manager, std::pmr::memory_resource* temp_resource = std::pmr::get_default_resource());
	
		~SingletonModifyManager();

	protected:

		struct Modify
		{
			BitFlag singleton_bitflag;
			Potato::IR::MemoryResourceRecord resource;
			StructLayout::Ptr singleton_class;
			bool Release();
		};

		std::pmr::vector<Modify> singleton_modify;

		BitFlagContainer singleton_modify_bitflag;
	};
}