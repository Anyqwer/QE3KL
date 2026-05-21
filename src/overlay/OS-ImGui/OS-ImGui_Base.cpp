#include "OS-ImGui_Base.h"
#include <locale>
#include <codecvt>
#include <d3d11.h>
#include <windows.h>  // For GetSystemMetrics

namespace OSImGui
{
    bool D3DDevice::CreateDeviceD3D(HWND hWnd)
    {
        
        // Step 1: Create D3D11 Device without SwapChain
        // BGRA_SUPPORT is CRITICAL for DirectComposition to work!
        // NOTE: DO NOT use D3D11_CREATE_DEVICE_DEBUG - causes crash on PCs without Graphics SDK
        UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

        HRESULT hr = D3D11CreateDevice(
            nullptr,                        // pAdapter = nullptr, so DriverType must be HARDWARE
            D3D_DRIVER_TYPE_HARDWARE,       // Required when pAdapter is nullptr
            nullptr,                        // Software rasterizer module
            createDeviceFlags,              // BGRA_SUPPORT only, NO DEBUG flag
            featureLevelArray,
            2,
            D3D11_SDK_VERSION,
            &g_pd3dDevice,
            &featureLevel,
            &g_pd3dDeviceContext);

        if (hr == DXGI_ERROR_UNSUPPORTED)
        {
            printf("[DX11] HARDWARE not supported, trying WARP...\n");
            hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
        }

        if (FAILED(hr))
        {
            printf("[DX11] D3D11CreateDevice failed: 0x%lX\n", hr);
            return false;
        }

        // Step 2: Get DXGI Device and Factory
        IDXGIDevice* dxgiDevice = nullptr;
        hr = g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
        if (FAILED(hr))
        {
            printf("[DX11] Failed to get IDXGIDevice: 0x%lX\n", hr);
            return false;
        }

        IDXGIAdapter* dxgiAdapter = nullptr;
        hr = dxgiDevice->GetAdapter(&dxgiAdapter);
        if (FAILED(hr))
        {
            printf("[DX11] Failed to get IDXGIAdapter: 0x%lX\n", hr);
            dxgiDevice->Release();
            return false;
        }

        IDXGIFactory2* dxgiFactory = nullptr;
        hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&dxgiFactory);
        dxgiAdapter->Release();  // Adapter no longer needed
        if (FAILED(hr))
        {
            printf("[DX11] Failed to get IDXGIFactory2: 0x%lX\n", hr);
            dxgiDevice->Release();
            return false;
        }

        // Step 3: Create Modern SwapChain with FLIP_DISCARD and Alpha
        
        // Get screen dimensions - CreateSwapChainForComposition REQUIRES non-zero size
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        
        DXGI_SWAP_CHAIN_DESC1 sd = {};
        sd.Width = screenWidth;   // CRITICAL: Must be non-zero for CreateSwapChainForComposition
        sd.Height = screenHeight; // CRITICAL: Must be non-zero for CreateSwapChainForComposition
        sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;  // Required for DComp
        sd.Stereo = FALSE;
        sd.SampleDesc.Count = 1;    // CRITICAL: Flip model requires 1 (no multisampling)
        sd.SampleDesc.Quality = 0;    // CRITICAL: Must be 0
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;  // CRITICAL: Required flag!
        sd.BufferCount = 2;           // CRITICAL: Minimum 2 for Flip model
        sd.Scaling = DXGI_SCALING_STRETCH;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;  // Modern flip model
        sd.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;   // Required for transparency
        sd.Flags = 0;
        

        hr = dxgiFactory->CreateSwapChainForComposition(
            g_pd3dDevice,
            &sd,
            nullptr,
            &g_pSwapChain);

        dxgiFactory->Release();
        if (FAILED(hr))
        {
            printf("[DX11] CreateSwapChainForComposition failed: 0x%lX\n", hr);
            dxgiDevice->Release();
            return false;
        }

        // Step 4: Initialize DirectComposition
        hr = DCompositionCreateDevice(
            dxgiDevice,  // Link with our D3D device
            __uuidof(IDCompositionDevice),
            (void**)&g_pDCompDevice);
        
        // Now safe to release dxgiDevice
        dxgiDevice->Release();
        
        if (FAILED(hr))
        {
            printf("[DCOMP] DCompositionCreateDevice failed: 0x%lX\n", hr);
            return false;
        }

        hr = g_pDCompDevice->CreateTargetForHwnd(hWnd, true, &g_pDCompTarget);
        if (FAILED(hr))
        {
            printf("[DCOMP] CreateTargetForHwnd failed: 0x%lX\n", hr);
            return false;
        }

        hr = g_pDCompDevice->CreateVisual(&g_pDCompVisual);
        if (FAILED(hr))
        {
            printf("[DCOMP] CreateVisual failed: 0x%lX\n", hr);
            return false;
        }

        hr = g_pDCompVisual->SetContent(g_pSwapChain);
        if (FAILED(hr))
        {
            printf("[DCOMP] SetContent failed: 0x%lX\n", hr);
            return false;
        }

        hr = g_pDCompTarget->SetRoot(g_pDCompVisual);
        if (FAILED(hr))
        {
            printf("[DCOMP] SetRoot failed: 0x%lX\n", hr);
            return false;
        }

        hr = g_pDCompDevice->Commit();
        if (FAILED(hr))
        {
            printf("[DCOMP] Commit failed: 0x%lX\n", hr);
            return false;
        }

        CreateRenderTarget();
        return true;
    }

    void D3DDevice::CleanupDeviceD3D()
    {
        CleanupRenderTarget();
        
        // Release DirectComposition resources (reverse order)
        if (g_pDCompVisual) { g_pDCompVisual->Release(); g_pDCompVisual = nullptr; }
        if (g_pDCompTarget) { g_pDCompTarget->Release(); g_pDCompTarget = nullptr; }
        if (g_pDCompDevice) { g_pDCompDevice->Release(); g_pDCompDevice = nullptr; }
        
        if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
        if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
        if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    }

    void D3DDevice::CreateRenderTarget()
    {
        ID3D11Texture2D* pBackBuffer;
        g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        if (pBackBuffer)
        {
            g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
            pBackBuffer->Release();
        }
    }

    void D3DDevice::CleanupRenderTarget()
    {
        if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
    }

    bool OSImGui_Base::InitImGui(ID3D11Device* device, ID3D11DeviceContext* device_context)
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGui::StyleColorsDark();

        // Load font with Cyrillic support for Russian names
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\arial.ttf", 16.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());

        if (!ImGui_ImplWin32_Init(Window.hWnd))
            return false;

        if (!ImGui_ImplDX11_Init(device, device_context))
            return false;

        // Setup premultiplied alpha blend state for DirectComposition
        // Required for proper transparency with DXGI_ALPHA_MODE_PREMULTIPLIED
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;           // Premultiplied alpha
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        ID3D11BlendState* blendState = nullptr;
        device->CreateBlendState(&blendDesc, &blendState);
        if (blendState)
        {
            float blendFactor[4] = { 0, 0, 0, 0 };
            device_context->OMSetBlendState(blendState, blendFactor, 0xffffffff);
            blendState->Release();
        }

        return true;
    }

    void OSImGui_Base::CleanImGui()
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    std::wstring OSImGui_Base::StringToWstring(std::string& str)
    {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return converter.from_bytes(str);
    }
}
