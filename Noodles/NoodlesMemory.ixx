module;

#include <cassert>

export module NoodlesMemory;

import std;
import PotatoPointer;
import PotatoMisc;
import PotatoIR;


export namespace Noodles::Memory
{

	template<typename MemoryResourceT>
		requires(std::is_base_of_v<std::pmr::memory_resource, MemoryResourceT>)
	struct IntrusiveMemoryResource
		: public MemoryResourceT, public Potato::Pointer::DefaultIntrusiveInterface
	{
		using Ptr = Potato::Pointer::IntrusivePtr<IntrusiveMemoryResource>;

		template<typename ...OT>
		static Ptr Create(std::pmr::memory_resource* resouece = std::pmr::get_default_resource(), OT&& ...ot);
		std::pmr::memory_resource* get_resource_interface() { return &static_cast<MemoryResourceT&>(*this);}

	protected:
		
		template<typename ...OT>
		IntrusiveMemoryResource(std::pmr::memory_resource* in_resouece, OT&& ...ot)
			: resource(in_resouece), MemoryResourceT(std::forward<OT>(ot)..., in_resouece)
		{
			
		}

		virtual void Release() override;
		virtual void* do_allocate(size_t byte, size_t align) override
		{
			DefaultIntrusiveInterface::AddRef();
			return MemoryResourceT::do_allocate(byte, align);
		}
		virtual void do_deallocate(void* adress, size_t byte, size_t align) override
		{
			MemoryResourceT::do_deallocate(adress, byte, align);
			DefaultIntrusiveInterface::SubRef();
		}
		std::pmr::memory_resource* resource = nullptr;
	};

	template<typename MemoryResourceT>
		requires(std::is_base_of_v<std::pmr::memory_resource, MemoryResourceT>)
	template<typename ...OT>
	auto IntrusiveMemoryResource<MemoryResourceT>::Create(std::pmr::memory_resource* up_stream, OT&& ...ot) -> Ptr
	{
		if (up_stream != nullptr)
		{
			auto Adress = up_stream->allocate(sizeof(IntrusiveMemoryResource), alignof(IntrusiveMemoryResource));
			if (Adress != nullptr)
			{
				return new (Adress) IntrusiveMemoryResource{ up_stream, std::forward<OT>(ot)...};
			}
		}
		return {};
	}

	template<typename MemoryResourceT>
		requires(std::is_base_of_v<std::pmr::memory_resource, MemoryResourceT>)
	void IntrusiveMemoryResource<MemoryResourceT>::Release()
	{
		assert(resource != nullptr);
		auto old_resource = resource;
		this->~IntrusiveMemoryResource();
		old_resource->deallocate(this, sizeof(IntrusiveMemoryResource), alignof(IntrusiveMemoryResource));
	}
}