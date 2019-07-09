// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include <Windows.h>
#include "..//..//Noodles/interface/interface.h"
#include "Dx11/context.h"
#include "..//..//Potato/document.h"
#include <iostream>
#include <array>
#include <mutex>

std::mutex* gobal_mutex;
using namespace Noodles;

template<typename Type> using ComPtr = Dx11::ComPtr<Type>;
using ComWrapper = Dx11::ComWrapper;

template<typename Type> struct CallRecord
{
	CallRecord()
	{
		std::lock_guard lg(*gobal_mutex);
		std::cout << "thread id<" << std::this_thread::get_id() << "> : " << typeid(Type).name() << " - start" << std::endl;
	}

	~CallRecord()
	{
		std::lock_guard lg(*gobal_mutex);
		std::cout << "thread id<" << std::this_thread::get_id() << "> : " << typeid(Type).name() << " - end" << std::endl;
	}
};

extern "C" {
	void __declspec(dllexport) init(Context*, std::mutex*);
}

struct Location
{
	float x, y;
};

struct Collision
{
	float Range;
};

struct Velocity
{
	float x, y;
};

struct EaterFlag {};

struct ProviderFlag {
	float Data = 0.0;
};

struct FoodFlag {};


struct RenderSystem
{
	void operator()(Filter<const Collision, const Location>& f, GobalFilter<Dx11::FormRenderer>& render)
	{
		CallRecord<RenderSystem> record;
		if (render->ready_to_update())
		{
			float p[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			(*render)->ClearRenderTargetView(*render, p);
			if (f.count() > 0)
			{
				size_t count = f.count();
				struct Poi
				{
					float lx, ly;
					float range, pro;
				};
				std::vector<Poi> data;
				data.resize(count);
				size_t i = 0;
				for (auto ite = f.begin(); ite != f.end(); ++ite)
				{
					auto& [col, loc] = *ite;
					auto entity = ite->entity();
					data[i].lx = loc.x;
					data[i].ly = loc.y;
					data[i].range = col.Range;
					if (entity.have<EaterFlag>())
						data[i].pro = 0.0f;
					else if(entity.have<ProviderFlag>())
						data[i].pro = 0.5f;
					else
						data[i].pro = 1.0f;
					++i;
				}
				D3D11_BUFFER_DESC DBD{ static_cast<UINT>(sizeof(Poi) * data.size()), D3D11_USAGE_IMMUTABLE , D3D11_BIND_VERTEX_BUFFER, 0, 0, static_cast<UINT>(sizeof(Poi)) };
				ComPtr<ID3D11Buffer> ins_buffer;
				D3D11_SUBRESOURCE_DATA DSD{ data.data(), 0, 0 };
				HRESULT re = (*context)->CreateBuffer(&DBD, &DSD, ComWrapper::ref(ins_buffer));
				assert(SUCCEEDED(re));

				(*render)->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				(*render)->IASetInputLayout(layout);
				ID3D11Buffer* tem_buffer[2] = { buffer, ins_buffer };
				UINT str[2] = { sizeof(float) * 2, sizeof(float) * 4 };
				UINT offset[2] = { 0, 0 };
				(*render)->IASetVertexBuffers(0, 2, tem_buffer, str, offset);
				(*render)->VSSetShader(vs, nullptr, 0);
				(*render)->PSSetShader(ps, nullptr, 0);
				ID3D11RenderTargetView* view[1] = { (*render) };
				(*render)->OMSetRenderTargets(1, view, nullptr);
				D3D11_VIEWPORT viewport{ 0.0, 0.0, 1024.0f, 768.0f, 0.0, 1.0 };
				(*render)->RSSetViewports(1, &viewport);
				(*render)->DrawInstanced(13 * 3, static_cast<UINT>(count), 0, 0);
			}
			(*render).replaceable_updates();
		}
	}
	RenderSystem(Dx11::ContextPtr input_context)
	{
		context = std::move(input_context);
		assert(context);
		struct Point
		{
			float x;
			float y;
		};
		std::array<Point, 13 * 3> all_buffer;
		for (size_t i = 0; i < 13; ++i)
		{
			all_buffer[i * 3] = Point{ 0.0f, 0.0f };
			all_buffer[i * 3 + 1] = Point{ sinf(3.141592653f * 2.0f / 13 * i), cos(3.141592653f * 2.0f / 13 * i) };
			all_buffer[i * 3 + 2] = Point{ sinf(3.141592653f * 2.0f / 13 * (i + 1)), cos(3.141592653f * 2.0f / 13 * (i + 1)) };
		}
		D3D11_BUFFER_DESC DBD{ sizeof(Point) * 13 * 3, D3D11_USAGE_IMMUTABLE , D3D11_BIND_VERTEX_BUFFER, 0, 0, sizeof(Point) };
		D3D11_SUBRESOURCE_DATA DSD{ all_buffer.data(), 0, 0 };
		HRESULT re = (*context)->CreateBuffer(&DBD, &DSD, ComWrapper::ref(buffer));
		assert(SUCCEEDED(re));
		Potato::Doc::loader_binary vsb_doc(L"VertexShader.cso");
		assert(vsb_doc.is_open());
		std::vector<std::byte> vsb;
		vsb.resize(vsb_doc.last_size());
		vsb_doc.read(vsb.data(), vsb.size());
		re = (*context)->CreateVertexShader(vsb.data(), vsb.size(), nullptr, ComWrapper::ref(vs));
		assert(SUCCEEDED(re));
		Potato::Doc::loader_binary psb_doc(L"PixelShader.cso");
		assert(psb_doc.is_open());
		std::vector<std::byte> psb;
		psb.resize(psb_doc.last_size());
		psb_doc.read(psb.data(), psb.size());
		re = (*context)->CreatePixelShader(psb.data(), psb.size(), nullptr, ComWrapper::ref(ps));
		assert(SUCCEEDED(re));
		D3D11_INPUT_ELEMENT_DESC input_desc[] = {
			D3D11_INPUT_ELEMENT_DESC {"VERPOSITION", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			D3D11_INPUT_ELEMENT_DESC {"POSITION", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			D3D11_INPUT_ELEMENT_DESC {"RANGE", 0, DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT, 1, sizeof(float) * 2, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			D3D11_INPUT_ELEMENT_DESC {"PROPERTY", 0, DXGI_FORMAT::DXGI_FORMAT_R32_FLOAT, 1, sizeof(float) * 3, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
		};
		re = (*context)->CreateInputLayout(input_desc, 4, vsb.data(), vsb.size(), ComWrapper::ref(layout));
		assert(SUCCEEDED(re));
	}
private:
	Dx11::ContextPtr context;
	ComPtr<ID3D11Buffer> buffer;
	ComPtr<ID3D11VertexShader> vs;
	ComPtr<ID3D11PixelShader> ps;
	ComPtr<ID3D11InputLayout> layout;
};

struct FormUpdateSystem
{
	void operator()(GobalFilter<Dx11::FormRenderer>& render, Context& c)
	{
		if (!render)
			return;
		CallRecord<FormUpdateSystem> record;
		MSG msg;
		while (render->pook_event(msg))
		{
			if (msg.message == WM_CLOSE)
			{
				c.exit();
				break;
			}
		}
	}

	TickOrder tick_order(const TypeInfo& layout, const TypeInfo*, size_t* count) const noexcept
	{
		if (layout == TypeInfo::create<RenderSystem>())
			return TickOrder::After;
		return TickOrder::Mutex;
	}
};

void init(Context* context, std::mutex* mutex)
{
	gobal_mutex = mutex;
	auto context2 = Dx11::Context::create();
	context->create_gobal_component<Dx11::FormRenderer>(context2->create_form());
	context->create_system<RenderSystem>(context2);
	context->create_system<FormUpdateSystem>();
}


























BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

