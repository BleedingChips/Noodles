#pragma once
#include <d3d11_4.h>
#include "form.h"
namespace Dx11
{
	struct ComWrapper
	{
		template<typename T> static void add_ref(T* com) { com->AddRef(); }
		template<typename T> static void sub_ref(T* com) { com->Release(); }
		template<typename T> static T** ref(Potato::Tool::intrusive_ptr<T, ComWrapper>& ptr) { assert(!ptr); return &ptr.m_ptr; }
	};

	template<typename T> using ComPtr = Potato::Tool::intrusive_ptr<T, ComWrapper>;

	struct ContextProperty
	{

	};

	struct FormProperty : Win32::FormProperty
	{
		DXGI_FORMAT format = DXGI_FORMAT::DXGI_FORMAT_R8G8B8A8_UNORM;
	};

	struct Context;
	using ContextPtr = Potato::Tool::intrusive_ptr<Context>;

	struct CommandInterface
	{
		virtual void add_ref() noexcept = 0;
		virtual void sub_ref() noexcept = 0;
		virtual bool apply(ID3D11DeviceContext& context) = 0;
	};

	struct DefaultCommandImplement : CommandInterface
	{
		virtual void add_ref() noexcept override;
		virtual void sub_ref() noexcept override;
		virtual void release() noexcept;
		virtual bool apply(ID3D11DeviceContext& context) override;
		ComPtr<ID3D11CommandList> m_command_list;
	private:
		Potato::Tool::atomic_reference_count m_ref;
	};

	struct Renderer
	{
		operator bool() const noexcept;
		Renderer(ComPtr<ID3D11DeviceContext> device, ContextPtr context);
		Renderer() = default;
		Renderer(Renderer&&) = default;
		Renderer& operator=(Renderer&&) = default;
		ID3D11DeviceContext* operator->() noexcept { return device_context; }
		void push_command();
	protected:
		ContextPtr m_context_ptr;
		ComPtr<ID3D11DeviceContext> device_context;
	};

	struct FormRenderer : Win32::Form, Renderer
	{
		operator bool() const noexcept;
		FormRenderer(Win32::Form form, ComPtr<IDXGISwapChain1> swap, ComPtr<ID3D11RenderTargetView> rtv, ComPtr<ID3D11DeviceContext> device, ContextPtr context);
		FormRenderer() = default;
		FormRenderer(FormRenderer&&) = default;
		FormRenderer& operator=(FormRenderer&&) = default;
		operator ID3D11RenderTargetView* () noexcept { return m_back_buffer_rtv; }
		void replaceable_updates() noexcept;
		bool ready_to_update() noexcept;
	private:
		struct Command : DefaultCommandImplement
		{
			virtual void release() noexcept override;
			virtual bool apply(ID3D11DeviceContext& context) override;
			std::mutex m_mutex;
			ComPtr<IDXGISwapChain1> m_swap_chain;
		};

		Potato::Tool::intrusive_ptr<Command> m_waitiing;
		Potato::Tool::intrusive_ptr<Command> m_buffer;

		ComPtr<IDXGISwapChain1> swap_chain;
		ComPtr<ID3D11RenderTargetView> m_back_buffer_rtv;
	};

	struct Context
	{
		void add_ref() noexcept;
		void sub_ref() noexcept;
		static ContextPtr create(const ContextProperty& pro = {});
		FormRenderer create_form(const FormProperty& pro = {});
		ID3D11Device* operator->() noexcept { return m_context; }
		~Context();
		void insert_command(CommandInterface* ins);
	private:
		Context(ComPtr<ID3D11Device>, ComPtr<ID3D11DeviceContext>);
		static void execution(Context* context);
		ComPtr<ID3D11Device> m_context;
		ComPtr<ID3D11DeviceContext> m_device_context;
		std::mutex m_list_mutex;
		std::deque<Potato::Tool::intrusive_ptr<CommandInterface>> m_list;
		std::atomic_bool m_available;
		std::thread m_execute_thread;

		Potato::Tool::atomic_reference_count m_ref;
	};
}