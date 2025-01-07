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

	template<AcceptableQueryType ...ComponentT>
	struct AutoComponentQuery
	{
		
		static std::span<StructLayoutWriteProperty const> GetRequire()
		{
			static std::array<StructLayoutWriteProperty, sizeof...(ComponentT)> temp_buffer = {
				StructLayoutWriteProperty::GetComponent<ComponentT>()...
			};
			return std::span(temp_buffer);
		}

		AutoComponentQuery(StructLayoutManager& manager, std::pmr::memory_resource* resource)
			: accessor(std::span(buffer))
		{
			query = ComponentQuery::Create(manager, GetRequire(), {}, resource);
			assert(query);
		}

		AutoComponentQuery(AutoComponentQuery const& in)
			: query(in.query), accessor(in.accessor, std::span(buffer))
		{
			
		}

		AutoComponentQuery(AutoComponentQuery&&) = delete;

		template<std::size_t index>
		decltype(auto) AsSpan() const
		{
			static_assert(index < sizeof...(ComponentT));
			using Type = Potato::TMP::FindByIndex<index, ComponentT...>::Type;
			return accessor.AsSpan<Type>(index);
		}

		template<typename Type>
		decltype(auto) AsSpan() const
		{
			static_assert(Potato::TMP::IsOneOfV<Type, ComponentT...>);
			constexpr std::size_t index = Potato::TMP::LocateByType<Type, ComponentT...>::Value;
			return accessor.AsSpan<Type>(index);
		}

		bool IterateComponent(Context& context, std::size_t ite_index) { return context.IterateComponent_AssumedLocked(*query, ite_index, accessor); }
		bool IterateComponent(ContextWrapper& context_wrapper, std::size_t ite_index) { return IterateComponent(context_wrapper.GetContext(), ite_index); }
		bool ReadEntity(Context& context, Entity const& entity) { return context.ReadEntity_AssumedLocked(entity, *query, accessor); }
		bool ReadEntity(ContextWrapper& context_wrapper, Entity const& entity) { return ReadEntity(context_wrapper.GetContext(), entity); }
		//decltype(auto) ReadEntityDirect_AssumedLocked(Context& context, Entity const& entity, std::span<void*> output, bool prefer_modifier = true) const { return context.ReadEntityDirect_AssumedLocked(entity, *filter, output, prefer_modifier); };

		template<AcceptableQueryType ...RefuseComponent>
		struct WithRefuse : public AutoComponentQuery<ComponentT...>
		{
			static std::span<StructLayout::Ptr const> GetResuse()
			{
				static std::array<StructLayout::Ptr, sizeof...(ComponentT)> temp_buffer = {
					Potato::IR::StructLayout::GetStatic<ComponentT>()...
				};
				return std::span(temp_buffer);
			}

			WithRefuse(
				StructLayoutManager& manager, std::pmr::memory_resource* resource) : AutoComponentQuery(
				ComponentQuery::Create(manager, GetRequire(), GetResuse(), resource)
				) {}

			WithRefuse(WithRefuse const& in) : AutoComponentQuery(in) {}
		};

		SystemNode::Mutex GetSystemNodeMutex() const
		{
			return {
				query->GetRequiredStructLayoutMarks(),
				{},
				{}
			};
		}

		SystemNode::ComponentOverlappingState IsComponentOverlapping(SystemNode const& target_node, std::span<MarkElement const> archetype_update,  std::span<MarkElement const> component_usage) const
		{
			if (MarkElement::IsOverlapping(query->GetArchetypeMarkArray(), archetype_update))
			{
				return target_node.IsComponentOverlapping(*query, archetype_update, component_usage);
			}else
			{
				return SystemNode::ComponentOverlappingState::NoUpdate;
			}
		}

		SystemNode::ComponentOverlappingState IsComponentOverlapping(ComponentQuery const& query, std::span<MarkElement const> archetype_update, std::span<MarkElement const> component_usage) const
		{
			if (MarkElement::IsOverlapping(this->query->GetArchetypeMarkArray(), archetype_update))
			{
				if (MarkElement::IsOverlappingWithMask(
					this->query->GetArchetypeMarkArray(),
					query.GetArchetypeMarkArray(),
					component_usage
				) && this->query->GetRequiredStructLayoutMarks().WriteConfig(query.GetRequiredStructLayoutMarks()))
				{
					return SystemNode::ComponentOverlappingState::IsOverlapped;
				}
				return SystemNode::ComponentOverlappingState::IsNotOverlapped;
			}
			else
			{
				return SystemNode::ComponentOverlappingState::NoUpdate;
			}
		}

		bool UpdateQuery(Context& context)
		{
			return context.UpdateQuery(*query);
		}

	protected:

		AutoComponentQuery(ComponentQuery::Ptr query) : query(query) {}

		ComponentQuery::Ptr query;
		std::array<void*, sizeof...(ComponentT)> buffer;
		QueryData accessor;

		friend struct Context;
	};

	template<AcceptableQueryType ...ComponentT>
	struct AutoSingletonQuery
	{
		static std::span<StructLayoutWriteProperty const> GetRequire()
		{
			static std::array<StructLayoutWriteProperty, sizeof...(ComponentT)> temp_buffer = {
				StructLayoutWriteProperty::GetSingleton<ComponentT>()...
			};
			return std::span(temp_buffer);
		}

		AutoSingletonQuery(StructLayoutManager& manager, std::pmr::memory_resource* resource)
			: accessor(std::span(buffers))
		{
			query = SingletonQuery::Create(
				manager,
				GetRequire(),
				resource
			);
			assert(query);
		}

		AutoSingletonQuery(AutoSingletonQuery const& other)
			: query(other.query), accessor(other.accessor, std::span(buffers))
		{
			assert(query);
		}

		decltype(auto) GetSingletons(Context& context) { return context.ReadSingleton_AssumedLocked(*query, accessor); }
		decltype(auto) GetSingletons(ContextWrapper& context_wrapper) { return GetSingletons(context_wrapper.GetContext()); }

		template<std::size_t index> auto Get() const
		{
			using Type = typename Potato::TMP::FindByIndex<index, ComponentT...>::Type;
			return accessor.AsSpan<Type>(index);
		}

		template<typename Type> Type* Get() const requires(Potato::TMP::IsOneOfV<Type, ComponentT...>)
		{
			constexpr std::size_t index = Potato::TMP::LocateByType<Type, ComponentT...>::Value;
			return accessor.AsSpan<Type>(index).data();
		}

		SystemNode::Mutex GetSystemNodeMutex() const
		{
			return {
				{},
				query->GetRequiredStructLayoutMarks(),
				{}
			};
		}

		bool UpdateQuery(Context& context)
		{
			return context.UpdateQuery(*query);
		}

	protected:

		SingletonQuery::Ptr query;
		std::array<void*, sizeof...(ComponentT)> buffers;
		QueryData accessor;
		friend struct Context;
	};

	template<AcceptableQueryType ...ComponentT>
	struct AutoThreadOrderQuery
	{
		AutoThreadOrderQuery(StructLayoutManager& manager, std::pmr::memory_resource* resource)
		{
			static std::array<StructLayoutWriteProperty, sizeof...(ComponentT)> temp_buffer = {
				StructLayoutWriteProperty::Get<ComponentT>()...
			};
			query = ThreadOrderQuery::Create(manager, std::span(temp_buffer), resource);
		}
		AutoThreadOrderQuery(AutoThreadOrderQuery const&) = default;
		AutoThreadOrderQuery(AutoThreadOrderQuery&&) = default;
		SystemNode::Mutex GetSystemNodeMutex() const
		{
			return { {}, {}, query->GetStructLayoutMarks() };
		}
	protected:
		ThreadOrderQuery::Ptr query;
	};


	template<typename Type>
	concept IsContextWrapper = std::is_same_v<std::remove_cvref_t<Type>, ContextWrapper>;

	template<typename Type>
	concept IsCoverFromContextWrapper = std::is_constructible_v<std::remove_cvref_t<Type>, ContextWrapper&>;

	template<typename Type>
	concept HasSystemMutexFunctionWrapper = requires(Type const& type)
	{
		{type.GetSystemNodeMutex()} -> std::same_as<SystemNode::Mutex>;
	};

	template<typename Type>
	concept HasIsComponentOverlappingWithSystemNodeFunctionWrapper = requires(Type const& type)
	{
		{ type.IsComponentOverlapping(std::declval<SystemNode const&>(), std::span<MarkElement const>{}, std::span<MarkElement const>{}) } -> std::same_as<SystemNode::ComponentOverlappingState>;
	};

	template<typename Type>
	concept HasIsComponentOverlappingWithComponentQueryFunctionWrapper = requires(Type const& type)
	{
		{ type.IsComponentOverlapping(std::declval<ComponentQuery const&>(), std::span<MarkElement const>{}, std::span<MarkElement const>{}) } -> std::same_as<SystemNode::ComponentOverlappingState>;
	};

	template<typename Type>
	concept HasUpdateQueryFunctionWrapper = requires(Type& type)
	{
		{ type.UpdateQuery(std::declval<Context&>()) } -> std::same_as<bool>;
	};

	struct SystemAutomatic
	{

		template<typename Type>
		struct ParameterHolder
		{
			using RealType = std::conditional_t<
				IsContextWrapper<Type>,
				Potato::TMP::ItSelf<void>,
				std::conditional_t<
				IsCoverFromContextWrapper<Type>,
				std::optional<std::remove_cvref_t<Type>>,
				std::remove_cvref_t<Type>
				>
			>;

			RealType data;

			ParameterHolder(StructLayoutManager& manager, std::pmr::memory_resource* resource)
				requires(std::is_constructible_v<RealType, StructLayoutManager&, std::pmr::memory_resource*>)
			: data(manager, resource) {
			}

			ParameterHolder(StructLayoutManager& manager, std::pmr::memory_resource* resource)
				requires(!std::is_constructible_v<RealType, StructLayoutManager&, std::pmr::memory_resource*>)
			{
			}

			decltype(auto) Translate(ContextWrapper& context)
			{
				if constexpr (IsContextWrapper<Type>)
					return context;
				else if constexpr (IsCoverFromContextWrapper<Type>)
				{
					assert(!data.has_value());
					data.emplace(context);
					return *data;
				}
				else
					return std::ref(data);
			}

			SystemNode::Mutex GetSystemNodeMutex() const
			{
				if constexpr (HasSystemMutexFunctionWrapper<RealType>)
				{
					return data.GetSystemNodeMutex();
				}else
				{
					return {};
				}
			}

			void Reset()
			{
				if constexpr (!IsContextWrapper<Type> && IsCoverFromContextWrapper<Type>)
				{
					assert(data.has_value());
					data.reset();
				}
			}

			SystemNode::ComponentOverlappingState IsComponentOverlapping(SystemNode const& target_node, std::span<MarkElement const> archetype_update, std::span<MarkElement const> component_usage) const
			{
				if constexpr (HasIsComponentOverlappingWithSystemNodeFunctionWrapper<RealType>)
				{
					return data.IsComponentOverlapping(target_node, archetype_update, component_usage);
				}
				return SystemNode::ComponentOverlappingState::NoUpdate;
			}

			SystemNode::ComponentOverlappingState IsComponentOverlapping(ComponentQuery const& query, std::span<MarkElement const> archetype_update, std::span<MarkElement const> component_usage) const
			{
				if constexpr (HasIsComponentOverlappingWithComponentQueryFunctionWrapper<RealType>)
				{
					return data.IsComponentOverlapping(query, archetype_update, component_usage);
				}
				return SystemNode::ComponentOverlappingState::NoUpdate;
			}

			bool UpdateQuery(Context& context)
			{
				if constexpr (HasUpdateQueryFunctionWrapper<RealType>)
				{
					return data.UpdateQuery(context);
				}
				return false;
			}
		};

		template<typename ...AT>
		struct ParameterHolders;

		template<typename Cur, typename ...AT>
		struct ParameterHolders<Cur, AT...>
		{
			ParameterHolder<Cur> cur_holder;
			ParameterHolders<AT...> other_holders;

			ParameterHolders(StructLayoutManager& manager, std::pmr::memory_resource* resource)
				: cur_holder(manager, resource), other_holders(manager, resource)
			{
			}

			decltype(auto) Get(std::integral_constant<std::size_t, 0>, ContextWrapper& context) { return cur_holder.Translate(context); }
			template<std::size_t i>
			decltype(auto) Get(std::integral_constant<std::size_t, i>, ContextWrapper& context) { return other_holders.Get(std::integral_constant<std::size_t, i - 1>{}, context); }

			void Reset()
			{
				cur_holder.Reset();
				other_holders.Reset();
			}

			void FlushSystemNodeMutex(StructLayoutMarksInfos component, StructLayoutMarksInfos singleton, StructLayoutMarksInfos thread_order) const
			{
				SystemNode::Mutex cur = cur_holder.GetSystemNodeMutex();
				if (cur.component_mark)
				{
					component.MarkFrom(cur.component_mark);
				}
				if (cur.singleton_mark)
				{
					singleton.MarkFrom(cur.singleton_mark);
				}
				if (cur.thread_order_mark)
				{
					thread_order.MarkFrom(cur.thread_order_mark);
				}
				other_holders.FlushSystemNodeMutex(component, singleton, thread_order);
			}

			SystemNode::ComponentOverlappingState IsComponentOverlapping(SystemNode::ComponentOverlappingState start_update_state, SystemNode const& target_node, std::span<MarkElement const> archetype_update, std::span<MarkElement const> component_usage) const
			{
				auto state = cur_holder.IsComponentOverlapping(target_node, archetype_update, component_usage);
				switch (state)
				{
				case SystemNode::ComponentOverlappingState::NoUpdate:
					return other_holders.IsComponentOverlapping(start_update_state, target_node, archetype_update, component_usage);
				case SystemNode::ComponentOverlappingState::IsNotOverlapped:
					return other_holders.IsComponentOverlapping(SystemNode::ComponentOverlappingState::IsNotOverlapped, target_node, archetype_update, component_usage);
				default:
					return SystemNode::ComponentOverlappingState::IsOverlapped;
				}
			}

			SystemNode::ComponentOverlappingState IsComponentOverlapping(SystemNode::ComponentOverlappingState start_update_state, ComponentQuery const& query, std::span<MarkElement const> archetype_update, std::span<MarkElement const> component_usage) const
			{
				auto state = cur_holder.IsComponentOverlapping(query, archetype_update, component_usage);
				switch (state)
				{
				case SystemNode::ComponentOverlappingState::NoUpdate:
					return other_holders.IsComponentOverlapping(start_update_state, query, archetype_update, component_usage);
				case SystemNode::ComponentOverlappingState::IsNotOverlapped:
					return other_holders.IsComponentOverlapping(SystemNode::ComponentOverlappingState::IsNotOverlapped, query, archetype_update, component_usage);
				default:
					return SystemNode::ComponentOverlappingState::IsOverlapped;
				}
			}

			bool UpdateQuery(Context& context)
			{
				bool value1 = cur_holder.UpdateQuery(context);
				bool value2 = other_holders.UpdateQuery(context);
				return value1 || value2;
			}
		};

		template<>
		struct ParameterHolders<>
		{
			ParameterHolders(StructLayoutManager& manager, std::pmr::memory_resource* resource) {}
			void Reset() {}
			SystemNode::ComponentOverlappingState IsComponentOverlapping(SystemNode::ComponentOverlappingState start_update_state, SystemNode const& target_node, std::span<MarkElement const> archetype_update, std::span<MarkElement const> component_usage) const { return start_update_state; }
			SystemNode::ComponentOverlappingState IsComponentOverlapping(SystemNode::ComponentOverlappingState start_update_state, ComponentQuery const& query, std::span<MarkElement const> archetype_update, std::span<MarkElement const> component_usage) const { return start_update_state; }
			void FlushSystemNodeMutex(StructLayoutMarksInfos component, StructLayoutMarksInfos singleton, StructLayoutMarksInfos thread_order) const {}
			bool UpdateQuery(Context& context) { return false; }
		};

		template<typename ...ParT>
		struct ExtractAppendDataForParameters
		{
			using Type = ParameterHolders<ParT...>;
			using Index = std::make_index_sequence<sizeof...(ParT)>;
		};

		template<typename Func>
		struct ExtractTickSystem
		{
			using Extract = typename Potato::TMP::FunctionInfo<std::remove_cvref_t<Func>>::template PackParameters<ExtractAppendDataForParameters>;

			using AppendDataT = typename Extract::Type;

			static auto Execute(ContextWrapper& context, AppendDataT& append_data, Func& func)
			{
				return Execute(context, append_data, func, typename Extract::Index{});
			}

			template<std::size_t ...i>
			static auto Execute(ContextWrapper& context, AppendDataT& append_data, Func& func, std::index_sequence<i...>)
			{
				struct Scope
				{
					AppendDataT& ref;
					~Scope() { ref.Reset(); }
				}Scope{ append_data };
				return std::invoke(
					func,
					append_data.Get(std::integral_constant<std::size_t, i>{}, context)...
				);
			}
		};
	};


	template<typename Func>
	struct DynamicAutoSystemHolder : public SystemNode, public Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		using Automatic = SystemAutomatic::ExtractTickSystem<Func>;

		typename Automatic::AppendDataT append_data;

		std::conditional_t<
			std::is_function_v<Func>,
			Func*,
			Func
		> fun;

		StructLayoutMarksInfos component;
		StructLayoutMarksInfos singleton;
		StructLayoutMarksInfos thread_core;

		DynamicAutoSystemHolder(
				StructLayoutManager& manager, Func fun, Potato::IR::MemoryResourceRecord record,
			StructLayoutMarksInfos component,
			StructLayoutMarksInfos singleton,
			StructLayoutMarksInfos thread_core
			)
			: append_data(manager, record.GetMemoryResource()), fun(std::move(fun)), MemoryResourceRecordIntrusiveInterface(record), component(component), singleton(singleton), thread_core(thread_core)
		{
			append_data.FlushSystemNodeMutex(component, singleton, thread_core);
		}

		virtual void SystemNodeExecute(ContextWrapper& context) override
		{
			if constexpr (std::is_function_v<Func>)
			{
				Automatic::Execute(context, append_data, *fun);
			}
			else
			{
				Automatic::Execute(context, append_data, fun);
			}
		}

		void AddSystemNodeRef() const override { MemoryResourceRecordIntrusiveInterface::AddRef(); }
		void SubSystemNodeRef() const override { MemoryResourceRecordIntrusiveInterface::SubRef(); }
		virtual SystemNode::ComponentOverlappingState IsComponentOverlapping(ComponentQuery const& component_query, std::span<MarkElement const> archetype_update, std::span<MarkElement const> archetype_usage_count) const override
		{
			return append_data.IsComponentOverlapping(SystemNode::ComponentOverlappingState::NoUpdate, component_query, archetype_update, archetype_usage_count);
		}
		virtual SystemNode::ComponentOverlappingState IsComponentOverlapping(SystemNode const& target_node, std::span<MarkElement const> archetype_update, std::span<MarkElement const> archetype_usage_count) const override
		{
			return append_data.IsComponentOverlapping(SystemNode::ComponentOverlappingState::NoUpdate, target_node, archetype_update, archetype_usage_count);
		}
		virtual SystemNode::Mutex GetMutex() const override
		{
			return {
				component,
				singleton,
				thread_core
			};
		}
		virtual bool UpdateQuery(Context& context) override { return append_data.UpdateQuery(context); }
	};

	template<typename Function>
	SystemNode::Ptr CreateAutomaticSystem(StructLayoutManager& manager, Function&& func, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
	{
		using Type = DynamicAutoSystemHolder<std::remove_cvref_t<Function>>;
		auto layout = Potato::MemLayout::MemLayoutCPP::Get<Type>();
		auto span_offset = layout.Insert(Potato::IR::Layout::Get<MarkElement>(), 
			(manager.GetComponentStorageCount()
			+ manager.GetSingletonStorageCount()
			+ manager.GetThreadOrderStorageCount()) * 2
		);
		auto re = Potato::IR::MemoryResourceRecord::Allocate(resource, layout.Get());
		if (re)
		{
			std::size_t ite_span_offset = span_offset;

			StructLayoutMarksInfos component{
				{new (re.GetByte(ite_span_offset)) MarkElement[manager.GetComponentStorageCount()],manager.GetComponentStorageCount()},
				{new (re.GetByte(ite_span_offset) + sizeof(MarkElement) * manager.GetComponentStorageCount()) MarkElement[manager.GetComponentStorageCount()], manager.GetComponentStorageCount()},
			};

			ite_span_offset += (manager.GetComponentStorageCount() * 2 * sizeof(MarkElement));

			StructLayoutMarksInfos singleton{
				{new (re.GetByte(ite_span_offset)) MarkElement[manager.GetSingletonStorageCount()], manager.GetSingletonStorageCount()},
				{new (re.GetByte(ite_span_offset) + sizeof(MarkElement) * manager.GetSingletonStorageCount()) MarkElement[manager.GetSingletonStorageCount()], manager.GetSingletonStorageCount()},
			};

			ite_span_offset += (manager.GetSingletonStorageCount() * 2 * sizeof(MarkElement));

			StructLayoutMarksInfos thread_order{
				{new (re.GetByte(ite_span_offset)) MarkElement[manager.GetThreadOrderStorageCount()], manager.GetThreadOrderStorageCount()},
				{new (re.GetByte(ite_span_offset) + sizeof(MarkElement) * manager.GetThreadOrderStorageCount()) MarkElement[manager.GetThreadOrderStorageCount()], manager.GetThreadOrderStorageCount()},
			};

			return new (re.Get()) Type{ manager, std::forward<Function>(func), re, component, singleton, thread_order };
		}
		return {};
	}


	template<typename Function>
	SystemNode::Ptr CreateAndAddAutomaticSystem(Context& context, Function&& func, Property property = {}, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
	{
		auto node = CreateAutomaticSystem(context.GetStructLayoutManager(), std::forward<Function>(func), resource);
		if (node)
		{
			if (context.AddTickedSystemNode(*node, std::move(property)))
			{
				return node;
			}
		}
		return {};
	}

	/*
	template<typename Func>
	struct SystemTemplateHolder : public SystemTemplate, public Potato::IR::MemoryResourceRecordIntrusiveInterface
	{
		Func func;
		virtual SystemNode::Ptr CreateSystemNode(Context& context, std::pmr::memory_resource* resource) const override
		{
			return context.CreateAutomaticSystem(func, resource);
		}
		SystemTemplateHolder(Potato::IR::MemoryResourceRecord record, Func&& func) : MemoryResourceRecordIntrusiveInterface(record), func(std::move(func)) {}
	protected:
		virtual void AddSystemTemplateRef() const override { MemoryResourceRecordIntrusiveInterface::AddRef(); }
		virtual void SubSystemTemplateRef() const override { MemoryResourceRecordIntrusiveInterface::SubRef(); }
	};
	*/

	/*
	template<typename Func>
	SystemTemplate::Ptr Context::CreateAutomaticSystemTemplate(Func&& func, std::pmr::memory_resource* resource)
		requires(std::is_copy_constructible_v<Func>)
	{
		using Type = std::remove_cvref_t<Func>;
		return Potato::IR::MemoryResourceRecord::AllocateAndConstruct<SystemTemplateHolder<Type>>(
			resource,
			std::forward<Func>(func)
		);
	}
	*/
}
