#include "context.h"
namespace Dx11
{

	void DefaultCommandImplement::add_ref() noexcept
	{
		m_ref.add_ref();
	}

	void DefaultCommandImplement::sub_ref() noexcept
	{
		if (m_ref.sub_ref())
			release();
	}

	void DefaultCommandImplement::release() noexcept
	{
		delete this;
	}

	bool DefaultCommandImplement::apply(ID3D11DeviceContext& context)
	{
		if (m_command_list)
		{
			context.ExecuteCommandList(m_command_list, FALSE);
			m_command_list = ComPtr<ID3D11CommandList>{};
			return true;
		}
		return false;
	}

	Renderer::operator bool() const noexcept { return device_context; }

	Renderer::Renderer(ComPtr<ID3D11DeviceContext> device, ContextPtr context)
		: device_context(std::move(device)), m_context_ptr(std::move(context)) {}

	void Renderer::push_command()
	{
		Potato::Tool::intrusive_ptr<DefaultCommandImplement> ptr = new DefaultCommandImplement{};
		HRESULT re = device_context->FinishCommandList(0, ptr->m_command_list());
		assert(SUCCEEDED(re));
		m_context_ptr->insert_command(ptr);
	}

	FormRenderer::operator bool() const noexcept
	{
		return Win32::Form::operator bool();
	}

	bool FormRenderer::ready_to_update() noexcept
	{
		if (m_waitiing->m_mutex.try_lock())
		{
			std::lock_guard lg(m_waitiing->m_mutex, std::adopt_lock);
			return !m_waitiing->m_command_list;
		}
		return false;
	}

	void FormRenderer::replaceable_updates() noexcept
	{
		
		ComPtr<ID3D11CommandList> command;
		HRESULT re = device_context->FinishCommandList(FALSE, command());
		assert(SUCCEEDED(re));
		if (m_waitiing->m_mutex.try_lock())
		{
			std::lock_guard lg(m_waitiing->m_mutex, std::adopt_lock);
			if (!m_waitiing->m_command_list)
			{
				m_waitiing->m_command_list = std::move(command);
				m_context_ptr->insert_command(m_waitiing);
				std::swap(m_waitiing, m_buffer);
			}
			else if(m_buffer->m_mutex.try_lock())
			{
				std::lock_guard lg(m_buffer->m_mutex, std::adopt_lock);
				m_buffer->m_command_list = std::move(command);
			}
			else
				assert(false);
		}
	}

	void FormRenderer::Command::release() noexcept
	{
		delete this;
	}

	bool FormRenderer::Command::apply(ID3D11DeviceContext& context)
	{
		std::lock_guard lg(m_mutex);
		if (DefaultCommandImplement::apply(context))
		{
			m_swap_chain->Present(0, 0);
			return true;
		}
		return false;
	}

	FormRenderer::FormRenderer(
		Win32::Form form, ComPtr<IDXGISwapChain1> swap, ComPtr<ID3D11RenderTargetView> rtv,
		ComPtr<ID3D11DeviceContext> device, ContextPtr context
	)
		: Win32::Form(std::move(form)), Renderer(std::move(device), std::move(context)), m_back_buffer_rtv(std::move(rtv)), swap_chain(std::move(swap))
	{
		m_waitiing = new Command{};
		m_waitiing->m_swap_chain = swap_chain;
		m_buffer = new Command{};
		m_buffer->m_swap_chain = swap_chain;
	}

	ContextPtr Context::create(const ContextProperty& pro)
	{
		D3D_FEATURE_LEVEL lel[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_11_1 };
		D3D_FEATURE_LEVEL final_level;
		ComPtr<ID3D11DeviceContext> device_context;
		ComPtr<ID3D11Device> device;
		HRESULT re = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			D3D11_CREATE_DEVICE_BGRA_SUPPORT |
			//D3D11_CREATE_DEVICE_DISABLE_GPU_TIMEOUT | 
			D3D11_CREATE_DEVICE_DEBUG,
			lel,
			1,
			D3D11_SDK_VERSION,
			device(),
			&final_level,
			device_context()
		);
		assert(SUCCEEDED(re));
		ContextPtr ptr = new Context{ std::move(device), std::move(device_context) };
		return std::move(ptr);
	}

	Context::Context(ComPtr<ID3D11Device> dev, ComPtr<ID3D11DeviceContext> context)
		: m_context(std::move(dev)), m_device_context(std::move(context)), m_available(true)
	{
		m_execute_thread = std::thread(execution, this);
	}

	void Context::add_ref() noexcept
	{
		m_ref.add_ref();
	}
	void Context::sub_ref() noexcept
	{
		if (m_ref.sub_ref())
			delete this;
	}

	Context::~Context()
	{
		m_available = false;
		m_execute_thread.join();
	}

	void Context::execution(Context* context)
	{
		while (context->m_available)
		{
			Potato::Tool::intrusive_ptr<CommandInterface> ptr;
			{
				std::lock_guard lg(context->m_list_mutex);
				if (!context->m_list.empty())
				{
					ptr = std::move(*context->m_list.begin());
					context->m_list.pop_front();
				}
			}
			if (ptr)
				ptr->apply(*context->m_device_context);
			else
				std::this_thread::sleep_for(std::chrono::milliseconds{ 1 });
		}
	}

	void Context::insert_command(CommandInterface* ins)
	{
		std::lock_guard lg(m_list_mutex);
		m_list.push_back(ins);
	}

	FormRenderer Context::create_form(const FormProperty& pro)
	{
		ComPtr<ID3D11DeviceContext> defer;
		HRESULT re;
		re = m_context->CreateDeferredContext(0, defer());
		assert(SUCCEEDED(re));
		
		DXGI_SWAP_CHAIN_DESC1 desc{
			static_cast<UINT>(pro.width) , static_cast<UINT>(pro.height),
			pro.format,
			FALSE,
			DXGI_SAMPLE_DESC{ 1, 0 },
			DXGI_USAGE_RENDER_TARGET_OUTPUT,
			1,
			DXGI_SCALING_STRETCH,
			DXGI_SWAP_EFFECT_DISCARD,
			DXGI_ALPHA_MODE_UNSPECIFIED,
			//DXGI_SWAP_CHAIN_FLAG::DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
			0
		};
		ComPtr<IDXGIDevice> pDXGIDevice;
		re = m_context->QueryInterface(__uuidof(IDXGIDevice), (void**)pDXGIDevice());
		assert(SUCCEEDED(re));
		ComPtr<IDXGIAdapter> pDXGIAdapter;
		re = pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void**)pDXGIAdapter());
		assert(SUCCEEDED(re));
		ComPtr<IDXGIFactory2> pIDXGIFactory2;
		re = pDXGIAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)pIDXGIFactory2());
		assert(SUCCEEDED(re));
		Win32::Form form = Win32::Form::create(pro);
		ComPtr<IDXGISwapChain1> result;
		re = pIDXGIFactory2->CreateSwapChainForHwnd(m_context, form, &desc, nullptr, nullptr, result());
		assert(SUCCEEDED(re));
		ComPtr<ID3D11Resource> resource;
		re = (result)->GetBuffer(0, __uuidof(ID3D11Resource), reinterpret_cast<void**>(resource()));
		assert(SUCCEEDED(re));
		D3D11_RENDER_TARGET_VIEW_DESC rt_des{ pro.format, D3D11_RTV_DIMENSION_TEXTURE2D };
		rt_des.Texture2D = D3D11_TEX2D_RTV{ 0 };
		ComPtr<ID3D11RenderTargetView> out;
		re = m_context->CreateRenderTargetView(resource, &rt_des, out());
		assert(SUCCEEDED(re));
		FormRenderer result_form{std::move(form), std::move(result), out, defer, this };
		return std::move(result_form);
	}

}