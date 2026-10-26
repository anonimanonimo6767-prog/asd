#pragma once
#include <d3d11.h>
#include <Windows.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

class Overlay {
public:
    HWND hwnd                       = nullptr;
    ID3D11Device*           device  = nullptr;
    ID3D11DeviceContext*    ctx     = nullptr;
    IDXGISwapChain*         swap    = nullptr;
    ID3D11RenderTargetView* rtv     = nullptr;

    bool Init(HWND target) {
        RECT r{};
        GetWindowRect(target, &r);
        int w = r.right - r.left;
        int h = r.bottom - r.top;

        WNDCLASSEXA wc{};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = DefWindowProcA;
        wc.lpszClassName = "v67overlay";
        wc.hInstance     = GetModuleHandle(nullptr);
        RegisterClassExA(&wc);

        hwnd = CreateWindowExA(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
            "v67overlay", "", WS_POPUP,
            r.left, r.top, w, h,
            nullptr, nullptr, wc.hInstance, nullptr
        );
        SetLayeredWindowAttributes(hwnd, RGB(0,0,0), 0, LWA_COLORKEY);
        ShowWindow(hwnd, SW_SHOW);

        DXGI_SWAP_CHAIN_DESC sd{};
        sd.BufferCount        = 2;
        sd.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.Width   = w;
        sd.BufferDesc.Height  = h;
        sd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow       = hwnd;
        sd.SampleDesc.Count   = 1;
        sd.Windowed           = TRUE;
        sd.SwapEffect         = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL fl;
        D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            nullptr, 0, D3D11_SDK_VERSION,
            &sd, &swap, &device, &fl, &ctx
        );

        ID3D11Texture2D* bb{};
        swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb);
        device->CreateRenderTargetView(bb, nullptr, &rtv);
        bb->Release();
        return true;
    }

    void BeginFrame() {
        float c[4] = {0,0,0,0};
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ctx->ClearRenderTargetView(rtv, c);
    }

    void EndFrame() { swap->Present(0,0); }
};
