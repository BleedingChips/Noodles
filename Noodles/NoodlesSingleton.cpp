module;

#include <cassert>

module NoodlesSingleton;

import NoodlesComponent;

namespace Noodles
{

	SingletonManager::SingletonManager(std::size_t singleton_container_count, Config config)
		: bitflag(singleton_container_count * 2, config.resource), singleton_resource(config.singleton_resource)
	{
		singleton_usage_bitflag = bitflag.AsSpan().subspan(0, singleton_container_count);
		singleton_update_bitflag = bitflag.AsSpan().subspan(singleton_container_count, singleton_container_count);
		description.resize(BitFlagContainerConstViewer::GetMaxBitFlagCount(singleton_container_count));
	}

	SingletonManager::~SingletonManager()
	{
		description.clear();
	}

	/*
	std::size_t SingletonManager::TranslateBitFlagToQueryData(std::span<BitFlag const> bitflag, std::span<std::size_t> output) const
	{
		std::size_t index = 0;
		if (output.size() >= bitflag.size())
		{
			for (auto ite : bitflag)
			{
				auto mv = singleton_archetype->FindMemberIndex(ite);
				if (mv)
				{
					++index;
					output[0] = singleton_archetype->GetMemberView(mv.Get()).member_layout.offset;
				}
				else {
					output[0] = std::numeric_limits<std::size_t>::max();
				}
				output = output.subspan(1);
			}
		}
		return index;
	}
	*/

	std::size_t SingletonManager::QuerySingletonData(std::span<BitFlag const> query_data, std::span<void*> output_singleton) const
	{
		std::size_t result = 0;
		if (output_singleton.size() >= query_data.size())
		{
			for (std::size_t index = 0; index < query_data.size(); ++index)
			{
				auto& ptr = description[query_data[index].value].struct_layout;
				output_singleton[index] = ptr ? ptr->GetObject() : nullptr;
			}
		}
		return result;
	}

	SingletonModifyManager::SingletonModifyManager(std::size_t singleton_container_count, std::pmr::memory_resource* resource)
		: singleton_modify(resource), singleton_modify_bitflag(singleton_container_count, resource)
	{
		singleton_modify.resize(BitFlagContainerConstViewer::GetMaxBitFlagCount(singleton_container_count));
	}

	bool SingletonModifyManager::AddSingleton(StructLayout const& singleton_class, BitFlag singleton_bitflag, bool is_move_construct, void* singleton_data, std::pmr::memory_resource* resource)
	{
		BitFlagContainerViewer viwer = singleton_modify_bitflag;
		auto index = singleton_bitflag.value;
		if (index < singleton_modify.size())
		{
			Potato::IR::StructLayoutObject::Ptr singleton_object;
			if (is_move_construct)
			{
				singleton_object = Potato::IR::StructLayoutObject::MoveConstruct(&singleton_class, singleton_data, resource);
			}
			else {
				singleton_object = Potato::IR::StructLayoutObject::CopyConstruct(&singleton_class, singleton_data, resource);
			}

			if (singleton_object)
			{
				auto& ref = singleton_modify[index];
				ref.type = ModifyType::Add;
				ref.singleton_object = std::move(singleton_object);
				BitFlagContainerViewer{ singleton_modify_bitflag }.SetValue(singleton_bitflag);
				return true;
			}
		}
		return false;
	}

	bool SingletonModifyManager::RemoveSingleton(BitFlag singleton_bitflag)
	{
		BitFlagContainerViewer viwer = singleton_modify_bitflag;
		auto index = singleton_bitflag.value;
		if (index < singleton_modify.size())
		{
			auto& ref = singleton_modify[index];
			if (ref.type == ModifyType::Add)
			{
				ref.singleton_object.Reset();
				ref.type = ModifyType::Remove;
				BitFlagContainerViewer{ singleton_modify_bitflag }.SetValue(singleton_bitflag);
				return true;
			}
			else if (ref.type == ModifyType::Empty)
			{
				ref.type = ModifyType::Remove;
				BitFlagContainerViewer{ singleton_modify_bitflag }.SetValue(singleton_bitflag);
				return true;
			}
		}
		return false;
	}

	bool SingletonModifyManager::FlushSingletonModify(SingletonManager& manager, std::pmr::memory_resource* temp_resource)
	{
		BitFlagContainerViewer viewer = singleton_modify_bitflag;
		bool done = false;
		manager.singleton_update_bitflag.Reset();

		for (std::size_t index = 0; index < singleton_modify.size() && index < manager.description.size(); ++index)
		{
			auto& ref = singleton_modify[index];
			if (ref.type == ModifyType::Remove)
			{
				auto& ref2 = manager.description[index];
				if (ref2.struct_layout)
				{
					ref2.struct_layout.Reset();
					manager.singleton_usage_bitflag.SetValue(BitFlag{ index }, false);
					manager.singleton_update_bitflag.SetValue(BitFlag{ index }, true);
					done = true;
				}
				ref.type = ModifyType::Empty;
				ref.singleton_object.Reset();
			}
			else if (ref.type == ModifyType::Add)
			{
				assert(ref.singleton_object);
				auto& ref2 = manager.description[index];
				if (!ref2.struct_layout)
				{
					ref2.struct_layout = Potato::IR::StructLayoutObject::MoveConstruct(*ref.singleton_object, {}, &manager.singleton_resource);
					assert(ref2.struct_layout);
					manager.singleton_usage_bitflag.SetValue(BitFlag{ index }, true);
					manager.singleton_update_bitflag.SetValue(BitFlag{ index }, true);
					done = true;
				}
				ref.type = ModifyType::Empty;
				ref.singleton_object.Reset();
			}
		}

		BitFlagContainerViewer{ singleton_modify_bitflag }.Reset();
		return done;
	}

	SingletonModifyManager::~SingletonModifyManager()
	{
		singleton_modify.clear();
	}
}