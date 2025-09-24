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
	}

	SingletonManager::~SingletonManager()
	{
		if (singleton_record)
		{
			assert(singleton_archetype);
			for (Archetype::MemberView const& ite : *singleton_archetype)
			{
				ite.struct_layout->Destruction(ite.offset.GetMember(singleton_record.GetByte()));
			}
			singleton_record.Deallocate();
			singleton_record = {};
			singleton_archetype.Reset();
		}
	}

	std::size_t SingletonManager::TranslateBitFlagToQueryData(std::span<BitFlag const> bitflag, std::span<std::size_t> output) const
	{
		std::size_t index = 0;
		if (singleton_archetype && output.size() >= bitflag.size())
		{
			for (auto ite : bitflag)
			{
				auto mv = singleton_archetype->FindMemberIndex(ite);
				if (mv)
				{
					++index;
					output[0] = singleton_archetype->GetMemberView(mv.Get()).offset.buffer_offset;
				}
				else {
					output[0] = std::numeric_limits<std::size_t>::max();
				}
				output = output.subspan(1);
			}
		}
		return index;
	}

	std::size_t SingletonManager::QuerySingletonData(std::span<std::size_t> query_data, std::span<void*> output_singleton) const
	{
		std::size_t result = 0;
		if (singleton_archetype && output_singleton.size() >= query_data.size())
		{
			while (!query_data.empty())
			{
				if (query_data[0] != std::numeric_limits<std::size_t>::max())
				{
					++result;
					output_singleton[0] = singleton_record.GetByte(query_data[0]);
				}
				else {
					output_singleton[0] = nullptr;
				}
				query_data = query_data.subspan(1);
				output_singleton = output_singleton.subspan(1);
			}
		}
		return result;
	}



	bool SingletonModifyManager::Modify::Release()
	{
		if (resource)
		{
			singleton_class->Destruction(resource.Get());
			singleton_class.Reset();
			resource.Deallocate();
			resource = {};
			return true;
		}
		return false;
	}

	SingletonModifyManager::SingletonModifyManager(std::size_t singleton_container_count, std::pmr::memory_resource* resource)
		: singleton_modify(resource), singleton_modify_bitflag(singleton_container_count, resource)
	{

	}

	bool SingletonModifyManager::AddSingleton(StructLayout const& singleton_class, BitFlag singleton_bitflag, bool is_move_construct, void* singleton_data, std::pmr::memory_resource* resource)
	{
		BitFlagContainerViewer viwer = singleton_modify_bitflag;

		auto old_value = viwer.GetValue(singleton_bitflag);
		if (old_value.has_value() && !*old_value)
		{
			auto record = Potato::IR::MemoryResourceRecord::Allocate(resource, singleton_class.GetLayout());
			if (record)
			{
				if (is_move_construct)
				{
					auto re = singleton_class.MoveConstruction(record.Get(), singleton_data);
					assert(re);
				}
				else {
					auto re = singleton_class.CopyConstruction(record.Get(), singleton_data);
					assert(re);
				}

				singleton_modify.emplace_back(
					singleton_bitflag,
					record,
					&singleton_class
				);

				auto re = viwer.SetValue(singleton_bitflag);
				assert(re && !*re);
				return true;
			}
		}
		return false;
	}

	bool SingletonModifyManager::RemoveSingleton(BitFlag singleton_bitflag)
	{

		BitFlagContainerViewer viewer = singleton_modify_bitflag;

		auto re = viewer.GetValue(singleton_bitflag);
		if (re.has_value() && *re)
		{
			viewer.SetValue(singleton_bitflag, false);
			auto find = std::find_if(singleton_modify.begin(), singleton_modify.end(), [=](Modify const& i1) {
				return i1.singleton_bitflag == singleton_bitflag;
				});
			if (find != singleton_modify.end())
			{
				find->Release();
				singleton_modify.erase(find);
			}
			return true;
		}
		return false;
	}

	bool SingletonModifyManager::FlushSingletonModify(SingletonManager& manager, std::pmr::memory_resource* temp_resource)
	{
		BitFlagContainerViewer viewer = singleton_modify_bitflag;
		bool done = false;
		manager.singleton_update_bitflag.Reset();

		if (singleton_modify.empty())
		{
			auto result = viewer.IsSame(manager.GetSingletonUsageBitFlagViewer());
			if (!result.has_value() || *result)
			{
				return false;
			}
		}

		auto result = viewer.IsSame(manager.GetSingletonUsageBitFlagViewer());
		assert(result.has_value());
		if(!*result)
		{
			if (!viewer.IsReset())
			{
				std::pmr::vector<Archetype::Init> init_list(temp_resource);
				std::pmr::vector<ComponentManager::Init> component_init_list(temp_resource);
				init_list.reserve(viewer.GetBitFlagCount());
				component_init_list.reserve(viewer.GetBitFlagCount());

				if (manager.singleton_archetype)
				{
					for (Archetype::MemberView const& ite : *manager.singleton_archetype)
					{
						auto re = viewer.GetValue(ite.bitflag);
						assert(re.has_value());
						if (*re)
						{
							init_list.emplace_back(ite.struct_layout, ite.bitflag);
							component_init_list.emplace_back(ite.bitflag, true, manager.singleton_record.GetByte(ite.offset.buffer_offset));
						}
					}
				}

				for (auto& ite : singleton_modify)
				{
					auto find = std::find_if(component_init_list.begin(), component_init_list.end(), [&](ComponentManager::Init& init) {
						return init.component_class == ite.singleton_bitflag;
						});
					if (find != component_init_list.end())
					{
						find->data = ite.resource.Get();
					}
					else {
						init_list.emplace_back(ite.singleton_class, ite.singleton_bitflag);
						component_init_list.emplace_back(ite.singleton_bitflag, true, ite.resource.Get());
					}
				}

				ComponentManager::Sort(init_list);

				auto archetype = Archetype::Create(singleton_modify_bitflag.AsSpan().size(), init_list, &manager.singleton_resource);

				if (archetype)
				{
					auto new_record = Potato::IR::MemoryResourceRecord::Allocate(&manager.singleton_resource, archetype->GetLayout());
					if (new_record)
					{
						for (auto& ite : component_init_list)
						{
							auto loc = archetype->FindMemberIndex(ite.component_class);
							assert(loc);
							auto& mm = (*archetype)[loc.Get()];
							auto re = mm.struct_layout->MoveConstruction(mm.offset.GetMember(new_record.GetByte()), ite.data);
							assert(re);
						}

						if (manager.singleton_archetype)
						{
							for (Archetype::MemberView const& ite : *manager.singleton_archetype)
							{
								ite.struct_layout->Destruction(
									ite.offset.GetMember(manager.singleton_record.GetByte())
								);
							}
							manager.singleton_record.Deallocate();
							manager.singleton_record = {};
						}


						manager.singleton_archetype = std::move(archetype);
						manager.singleton_record = new_record;
						done = true;
					}
				}
			}
			else {
				assert(manager.singleton_archetype);
				for (Archetype::MemberView const& ite : *manager.singleton_archetype)
				{
					ite.struct_layout->Destruction(
						ite.offset.GetMember(manager.singleton_record.GetByte())
					);
				}
				done = true;
			}

		}
		else {
			assert(manager.singleton_archetype);
			for (auto& ite : singleton_modify)
			{
				auto loc = manager.singleton_archetype->FindMemberIndex(ite.singleton_bitflag);
				assert(loc);
				Archetype::MemberView const& mv = manager.singleton_archetype->GetMemberView()[loc.Get()];
				auto re = mv.struct_layout->Destruction(
					mv.offset.GetMember(manager.singleton_record.GetByte())
				);
				assert(re);
				re = mv.struct_layout->MoveConstruction(
					mv.offset.GetMember(manager.singleton_record.GetByte()),
					ite.resource.Get()
				);
				assert(re);
			}
			done = true;
		}

		if (done)
		{
			++manager.version;
			auto re = manager.singleton_update_bitflag.ExclusiveOr(manager.singleton_usage_bitflag, singleton_modify_bitflag);
			assert(re);
			re = manager.singleton_usage_bitflag.CopyFrom(singleton_modify_bitflag);
			for (auto& ite : singleton_modify)
			{
				ite.Release();
			}
			singleton_modify.clear();
			return true;
		}

		return false;
	}

	SingletonModifyManager::~SingletonModifyManager()
	{
		for (auto& ite : singleton_modify)
		{
			ite.Release();
		}
		singleton_modify.clear();
	}
}