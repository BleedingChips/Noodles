module;

#include <cassert>

export module NoodlesAutomaticSystem;

import std;

import Potato;

import NoodlesMisc;
import NoodlesArchetype;
import NoodlesComponent;
import NoodlesQuery;
import NoodlesContext;
import NoodlesEntity;


export namespace Noodles
{
	
	struct AutoSystemContext
	{
		std::span<ComponentQuery::OPtr> component;
		std::span<SingletonQuery::OPtr> singleton;
	};

	struct AutoComponentQueryIterator
	{
		std::size_t archetype_iterator = 0;
		std::size_t chunk_iterator = 0;
	};

	export template<AcceptableQueryType ...ComponentT>
		struct AutoComponentQuery;

	export template<typename Require, typename Refuse>
		struct AutoComponentQueryWrapper;

	template<typename RequireTypeTuple, typename RefuseTypeTuple>
	struct AutoComponentQueryWithRefuse;

	template<AcceptableQueryType ...Require, AcceptableQueryType ...Refuse>
	struct AutoComponentQueryWrapper<Potato::TMP::TypeTuple<Require...>, Potato::TMP::TypeTuple<Refuse...>>
	{
		void Init(SystemInitializer& initializer)
		{
			initializer.CreateComponentQuery(
				sizeof...(Require),
				[](ComponentQueryInitializer& init) {
					
					static std::array<
						std::tuple<Potato::IR::StructLayout::Ptr, bool>,
						sizeof...(Require)
					> static_require_struct_layout = { std::tuple<Potato::IR::StructLayout::Ptr, bool>{Potato::IR::StructLayout::GetStatic<Require>(), IsQueryWriteType<Require>}...};
					
					for (auto& ite : static_require_struct_layout)
					{
						init.SetRequire(std::get<0>(ite), std::get<1>(ite));
					}
					
					static std::array<
						Potato::IR::StructLayout::Ptr,
						sizeof...(Refuse)
					> static_refuse_struct_layout = { Potato::IR::StructLayout::GetStatic<Refuse>()...};

					for (auto& ite : static_refuse_struct_layout)
					{
						init.SetRefuse(ite);
					}
				}
			);
		}

		auto Translate(Context& context, AutoSystemContext& sys_context)
		{
			assert(sys_context.component.size() > 0);
			if constexpr (sizeof...(Refuse) == 0)
			{
				AutoComponentQuery<Require...> result{ context, sys_context.component[0] };
				sys_context.component = sys_context.component.subspan(1);
				return result;
			}
			else {
				AutoComponentQueryWithRefuse<Potato::TMP::TypeTuple<Require...>, Potato::TMP::TypeTuple<Refuse...>> result{ context, sys_context.component[0] };
				sys_context.component = sys_context.component.subspan(1);
				return result;
			}
			
		}
	};

	template<std::size_t start, std::size_t end>
	struct QueryDataWrapperHelper
	{
		template<typename Tuple>
		static void Set(Tuple& tuple, std::span<void*> source, std::size_t count)
		{
			using Type = typename std::tuple_element<start, Tuple>::type::value_type;
			auto source_ptr = static_cast<Type*>(source[start]);
			if (source_ptr == nullptr)
			{
				std::get<start>(tuple) = { };
			}
			else {
				std::get<start>(tuple) = { static_cast<Type*>(source[start]), count };
			}
			QueryDataWrapperHelper<start + 1, end>::Set(tuple, source, count);
		}
	};

	template<std::size_t end>
	struct QueryDataWrapperHelper<end, end>
	{
		template<typename Tuple>
		static void Set(Tuple& tuple, std::span<void*> source, std::size_t count){}
	};

	template<AcceptableQueryType... ComponentT>
	struct QueryDataWrapper
	{
		std::array<void*, sizeof...(ComponentT)> raw_query_data = {(Potato::TMP::ItSelf<ComponentT>(), nullptr)...};
		std::size_t span_count = 0;
		std::tuple<std::span<ComponentT>...> query_data;
		void Flush()
		{
			QueryDataWrapperHelper<0, sizeof...(ComponentT)>::Set(query_data, raw_query_data, span_count);
		}
		template<std::size_t index>
		decltype(auto) Get() { return std::get<index>(query_data); }
	};

	export template<AcceptableQueryType ...ComponentT>
		struct AutoComponentQuery
	{
		using Wrapper = AutoComponentQueryWrapper<Potato::TMP::TypeTuple<ComponentT...>, Potato::TMP::TypeTuple<>>;
		AutoComponentQuery(AutoComponentQuery&&) = default;
		AutoComponentQuery(Context& context, ComponentQuery::OPtr query)
			: context(context), query(std::move(query)) {}

		template<AcceptableQueryType ...RefuseComponentT>
		using Refuse = AutoComponentQueryWithRefuse<
			Potato::TMP::TypeTuple<ComponentT...>,
			Potato::TMP::TypeTuple<RefuseComponentT...>
		>;

		using Data = QueryDataWrapper<ComponentT...>;

		Data GetQueryData() { return {}; }

		bool QuerySpanAndMoveToNext(Context& context, AutoComponentQueryIterator& iterator, Data& output)
		{
			while (true)
			{
				auto entity_count = context.QueryComponentArray(*query, iterator.archetype_iterator, iterator.chunk_iterator, output.raw_query_data);
				if (entity_count.has_value())
				{
					if (*entity_count != 0)
					{
						output.span_count = *entity_count;
						output.Flush();
						iterator.chunk_iterator += 1;
						return true;
					}
					else {
						iterator.chunk_iterator = 0;
						iterator.archetype_iterator += 1;
					}
				}
				else {
					return false;
				}
			}
		}
		bool QueryEntity(Context& context, Entity const& entity, Data& output)
		{
			if (context.QueryEntity(*query, entity, output.raw_query_data))
			{
				output.span_count = 1;
				output.Flush();
				return true;
			}
			else {
				return false;
			}
		}
	protected:
		Context& context;
		ComponentQuery::OPtr query;
	};

	template<typename ...RequireTypeTuple, typename ...RefuseTypeTuple>
	struct AutoComponentQueryWithRefuse<Potato::TMP::TypeTuple<RequireTypeTuple...>, Potato::TMP::TypeTuple<RefuseTypeTuple...>>
		: public AutoComponentQuery<RequireTypeTuple...>
	{
		using Wrapper = AutoComponentQueryWrapper<Potato::TMP::TypeTuple<RequireTypeTuple...>, Potato::TMP::TypeTuple<RefuseTypeTuple...>>;
		AutoComponentQueryWithRefuse(AutoComponentQueryWithRefuse&&) = default;
		AutoComponentQueryWithRefuse(Context& context, ComponentQuery::OPtr query)
			: AutoComponentQuery<RequireTypeTuple...>(context, std::move(query))
		{
		}
	};


	export template<AcceptableQueryType ...SingletonT>
		struct AutoSingletonQuery;

	template<AcceptableQueryType ...RequireSingletonT>
	struct AutoSingletonQueryWrapper
	{
		void Init(SystemInitializer& initializer)
		{
			initializer.CreateSingletonQuery(
				sizeof...(RequireSingletonT),
				[](SingletonQueryInitializer& init) {

					static std::array<
						std::tuple<Potato::IR::StructLayout::Ptr, bool>,
						sizeof...(RequireSingletonT)
					> static_require_struct_layout = { std::tuple<Potato::IR::StructLayout::Ptr, bool>{Potato::IR::StructLayout::GetStatic<RequireSingletonT>(), IsQueryWriteType<RequireSingletonT>}... };

					for (auto& ite : static_require_struct_layout)
					{
						init.SetRequire(std::get<0>(ite), std::get<1>(ite));
					}
				}
			);
		}

		auto Translate(Context& context, AutoSystemContext& sys_context)
		{
			assert(sys_context.singleton.size() > 0);
			AutoSingletonQuery<RequireSingletonT...> result{ context, sys_context.singleton[0] };
			sys_context.singleton = sys_context.singleton.subspan(1);
			return result;
		}
	};

	export template<AcceptableQueryType ...SingletonT>
		struct AutoSingletonQuery
	{
		using Wrapper = AutoSingletonQueryWrapper<SingletonT...>;
		AutoSingletonQuery(AutoSingletonQuery&&) = default;
		AutoSingletonQuery(Context& context, SingletonQuery::OPtr query)
			: context(context), query(std::move(query)) {
		}

		using Data = QueryDataWrapper<SingletonT...>;

		Data GetQueryData() { return {}; }

		bool Query(Context& context, Data& output)
		{
			if (context.QuerySingleton(*query, output.raw_query_data))
			{
				output.span_count = 1;
				output.Flush();
				return true;
			}
			return false;
		}

	protected:
		Context& context;
		SingletonQuery::OPtr query;
	};

	struct ContextWrapper
	{
		void Init(SystemInitializer& initializer) {};
		Context& Translate(Context& context, AutoSystemContext& auto_context)
		{
			return context;
		}
	};

	template<typename TargetType>
	struct AutoSystemWrapperGetter
	{
		using Type = TargetType;
	};

	template<typename ...TargetType>
	struct AutoSystemWrapperGetter<AutoComponentQuery<TargetType...>>
	{
		using Type = AutoComponentQuery<TargetType...>::Wrapper;
	};

	template<typename TargetType, typename RefuseType>
	struct AutoSystemWrapperGetter<AutoComponentQueryWithRefuse<TargetType, RefuseType>>
	{
		using Type = typename AutoComponentQueryWithRefuse<TargetType, RefuseType>::Wrapper;
	};

	template<typename ...TargetType>
	struct AutoSystemWrapperGetter<AutoSingletonQuery<TargetType...>>
	{
		using Type = AutoSingletonQuery<TargetType...>::Wrapper;
	};

	template<>
	struct AutoSystemWrapperGetter<Context>
	{
		using Type = ContextWrapper;
	};

	template<typename ...WrapperT>
	struct AutoSystemWrapperList;

	template<>
	struct AutoSystemWrapperList<>
	{
		static constexpr std::size_t ParameterCount = 0;

		void Init(SystemInitializer& initializer){}

		template<typename Func, typename ...OtherParameter>
		void Execute(Context& context, AutoSystemContext& auto_context, Func&& func, OtherParameter&& ...para)
		{
			std::forward<Func>(func)(std::forward<OtherParameter>(para)...);
		}
	};

	template<typename ThisWrapperT, typename ...OtherWrapperT>
	struct AutoSystemWrapperList<ThisWrapperT, OtherWrapperT...> : public AutoSystemWrapperList<OtherWrapperT...>
	{
		static constexpr std::size_t ParameterCount = sizeof...(OtherWrapperT) + 1;
		using Type = AutoSystemWrapperGetter<std::remove_cvref_t<ThisWrapperT>>::Type;

		Type wrapper;


		void Init(SystemInitializer& initializer)
		{
			wrapper.Init(initializer);
			AutoSystemWrapperList<OtherWrapperT...>::Init(initializer);
		}

		template<typename Func, typename ...OtherParameter>
		void Execute(Context& context, AutoSystemContext& auto_context, Func&& func, OtherParameter&& ...para)
		{
			AutoSystemWrapperList<OtherWrapperT...>::Execute(
				context, auto_context, std::forward<Func>(func), std::forward<OtherParameter>(para)..., wrapper.Translate(context, auto_context)
			);
		}
	};
	

	template<typename Func>
	struct AutoSystemNode : public SystemNode, public Potato::IR::MemoryResourceRecordIntrusiveInterface
	{

		AutoSystemNode(Potato::IR::MemoryResourceRecord record, Func&& func)
			: MemoryResourceRecordIntrusiveInterface(record), func(std::move(func)) 
		{

		}

		using WrapperListType = typename Potato::TMP::FunctionInfo<std::remove_cvref_t<Func>>::template PackParameters<AutoSystemWrapperList>;

		WrapperListType wrappers;
		Func func;

		virtual void Init(SystemInitializer& initializer) override
		{
			wrappers.Init(initializer);
		}

		virtual void SystemNodeExecute(Context& context) override
		{
			if constexpr (WrapperListType::ParameterCount != 0)
			{
				std::array<ComponentQuery::OPtr, WrapperListType::ParameterCount> component_list;
				std::array<SingletonQuery::OPtr, WrapperListType::ParameterCount> singleton_list;

				auto [cc, sc] = context.GetQuery(std::span(component_list), std::span(singleton_list));

				AutoSystemContext auto_context{
					std::span(component_list).subspan(0, cc),
					std::span(singleton_list).subspan(0, sc)
				};

				wrappers.Execute(context, auto_context, func);
			}
			else {
				AutoSystemContext auto_context{ {}, {} };

				wrappers.Execute(context, auto_context, func);
			}
		}

		virtual void AddSystemNodeRef() const { MemoryResourceRecordIntrusiveInterface::AddRef(); }
		virtual void SubSystemNodeRef() const { MemoryResourceRecordIntrusiveInterface::SubRef(); }
	};




	template<typename Func>
	SystemNode::Ptr CreateAutoSystemNode(Func&& func, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
	{
		using Type = AutoSystemNode<Func>;

		auto re = Potato::IR::MemoryResourceRecord::Allocate<Type>(resource);
		if (re)
		{
			return new(re.Get()) Type{re, std::forward<Func>(func)};
		}
		return {};
	}
}
