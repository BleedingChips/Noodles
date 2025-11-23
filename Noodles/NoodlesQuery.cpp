module;

#include <cassert>

module NoodlesQuery;

namespace Noodles
{

	ComponentQuery::Ptr ComponentQuery::Create(
		std::size_t archetype_container_count,
		std::size_t component_container_count,
		std::size_t require_count,
		void (*init_func)(void*, std::span<BitFlag> require, BitFlagContainerViewer writed, BitFlagContainerViewer refuse),
		void* append_data,
		std::pmr::memory_resource* resource
	)
	{
		auto layout = Potato::IR::Layout::Get<ComponentQuery>();
		auto layout_policy = Potato::IR::PolicyLayout{ layout };
		auto require_offset = *layout_policy.Combine(Potato::IR::Layout::Get<BitFlag>(), require_count);
		auto bitflag_offset = *layout_policy.Combine(Potato::IR::Layout::Get<BitFlagContainerViewer::Element>(), component_container_count * 3 + archetype_container_count);
		
		auto record = Potato::IR::MemoryResourceRecord::Allocate(resource, *layout_policy.Complete());

		if (record)
		{

			auto require_span = std::span<BitFlag>{
				new (require_offset.GetMember(record.GetByte())) BitFlag[require_count],
				require_count
			};

			auto bitflag_conatiner_span = std::span<BitFlagContainerViewer::Element>{
				new (bitflag_offset.GetMember(record.GetByte())) std::size_t[component_container_count * 3 + archetype_container_count],
				component_container_count * 3 + archetype_container_count
			};

			BitFlagContainerViewer require = bitflag_conatiner_span.subspan(0, component_container_count);
			BitFlagContainerViewer refuse = bitflag_conatiner_span.subspan(component_container_count, component_container_count);
			BitFlagContainerViewer writed = bitflag_conatiner_span.subspan(component_container_count * 2, component_container_count);
			BitFlagContainerViewer archetype_usage = bitflag_conatiner_span.subspan(component_container_count * 3, archetype_container_count);
			require.Reset();
			refuse.Reset();
			writed.Reset();
			archetype_usage.Reset();

			(*init_func)(append_data, require_span, writed, refuse);

			for (auto ite : require_span)
			{
				auto re = require.SetValue(ite);
				assert(re);
			}

			assert(*require.Inclusion(writed));

			return new (record.Get()) ComponentQuery{ record, require, writed, refuse, archetype_usage, require_span };
		}

		return {};
	}

	bool ComponentQuery::UpdateQueryData(ComponentManager const& manager)
	{
		if (updated_archetype_count < manager.GetArchetypeCount())
		{
			bool has_been_modify = false;
			for (std::size_t index = updated_archetype_count; index < manager.GetArchetypeCount(); ++index)
			{
				if (manager.IsArchetypeAcceptQuery(index, require_bitflag_viewer, refuse_bitflag_viewer))
				{
					has_been_modify = true;
					++archetype_count;
					auto old_size = query_data.size();
					query_data.resize(query_data.size() + 1 + require_bitflag.size() * ComponentManager::GetQueryDataCount());
					query_data[old_size] = index;
					auto re = manager.TranslateClassToQueryData(index, require_bitflag, std::span(query_data).subspan(old_size + 1));
					assert(re);
					archetype_bitflag_viewer.SetValue(BitFlag{index});
				}
			}
			updated_archetype_count = manager.GetArchetypeCount();
			return has_been_modify;
		}
		return false;
	}

	std::optional<std::size_t> ComponentQuery::QueryComponentArrayWithIterator(ComponentManager& manager, std::size_t iterator, std::size_t chunk_index, std::span<void*> output_component) const
	{
		if (iterator < archetype_count)
		{
			auto span = std::span<std::size_t const>(query_data).subspan(query_data_fast_offset * iterator, query_data_fast_offset);
			auto re = manager.QueryComponentArray(span[0], chunk_index, span.subspan(1), output_component);
			if (re)
			{
				return re.Get();
			}
		}
		return {};
	}

	std::optional<std::size_t> ComponentQuery::GetChunkCount(ComponentManager& manager, std::size_t iterator) const
	{
		if (iterator < archetype_count)
		{
			auto span = std::span<std::size_t const>(query_data).subspan(query_data_fast_offset * iterator, query_data_fast_offset);
			auto re =  manager.GetChunkCount(span[0]);
			if (re)
			{
				return re.Get();
			}
		}
		return {};
	}

	bool ComponentQuery::QueryComponent(ComponentManager& manager, ComponentManager::Index entity_index, std::span<void*> output_component) const
	{
		auto re = archetype_bitflag_viewer.GetValue(BitFlag{entity_index.archetype_index});
		if (re.has_value() && *re)
		{
			auto span = std::span<std::size_t const>(query_data);
			for (std::size_t i = 0; i < archetype_count; ++i)
			{
				auto ite_span = span.subspan(query_data_fast_offset * i, query_data_fast_offset);
				if (entity_index.archetype_index == ite_span[0])
				{
					return manager.QueryComponent(entity_index, ite_span.subspan(1), output_component);
				}
			}
		}
		return false;
	}

	SingletonQuery::Ptr SingletonQuery::Create(
		std::size_t singleton_container_count,
		std::size_t singleton_count,
		void (*init_func)(void*, std::span<BitFlag> require, BitFlagContainerViewer writed),
		void* append_data,
		std::pmr::memory_resource* resource
	)
	{
		auto layout = Potato::IR::Layout::Get<ComponentQuery>();
		auto layout_policy = Potato::IR::PolicyLayout{ layout };
		auto require_offset = *layout_policy.Combine(Potato::IR::Layout::Get<BitFlag>(), singleton_count);
		auto bitflag_offset = *layout_policy.Combine(Potato::IR::Layout::Get<BitFlagContainerViewer::Element>(), singleton_container_count * 2);


		
		auto record = Potato::IR::MemoryResourceRecord::Allocate(resource, *layout_policy.Complete());

		if (record)
		{

			auto require_span = std::span<BitFlag>{
				new (require_offset.GetMember(record.GetByte())) BitFlag[singleton_count],
				singleton_count
			};

			auto bitflag_conatiner_span = std::span<BitFlagContainerViewer::Element>{
				new (bitflag_offset.GetMember(record.GetByte())) std::size_t[singleton_container_count * 2],
				singleton_container_count * 2
			};

			BitFlagContainerViewer require = bitflag_conatiner_span.subspan(0, singleton_container_count);
			BitFlagContainerViewer writed = bitflag_conatiner_span.subspan(singleton_container_count, singleton_container_count);
			require.Reset();
			writed.Reset();

			(*init_func)(append_data, require_span, writed);

			for (auto ite : require_span)
			{
				auto re = require.SetValue(ite);
				assert(re);
			}

			assert(*require.Inclusion(writed));

			return new (record.Get()) SingletonQuery{ record, require, writed, require_span };
		}

		return {};
	}

	bool SingletonQuery::QuerySingleton(SingletonManager const& manager, std::span<void*> output_component) const
	{
		manager.QuerySingletonData(singleton_bitflag, output_component);
		return true;
	}
}