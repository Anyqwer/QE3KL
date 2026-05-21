#include "OS-ImGui_External.h"
#include "../menu_config.hpp"
#include <windows.h>
#include <dwmapi.h>
#include <thread>
#include <chrono>
#include <intrin.h>

// Функция автоопределения герцовки монитора
int GetMonitorRefreshRate() {
    DEVMODE dm;
    dm.dmSize = sizeof(DEVMODE);
    
    // Получаем текущие настройки дисплея
    if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &dm)) {
        return dm.dmDisplayFrequency;  // Герцовка в Гц
    }
    return 60;  // Значение по умолчанию
}

// Forward declaration for ImGui WndProc handler
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace OSImGui
{
    // Custom WndProc to handle ImGui input
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        // Let ImGui handle the input first
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        // Handle window messages
        switch (msg)
        {
        case WM_NCHITTEST:
        {
            // If menu is open, allow interaction with the overlay
            if (esp::g_menu_config.ShowMenu) {
                return HTCLIENT;
            }
            // Otherwise, make entire window transparent for clicks (pass through to CS2)
            return HTTRANSPARENT;
        }
        case WM_SIZE:
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_KEYMENU) // Disable ALT menu
                return 0;
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    void OSImGui_External::NewWindow(std::string WindowName, Vec2 WindowSize, std::function<void()> CallBack)
    {
        Type = NEW;
        Window.Name = WindowName;
        Window.Size = WindowSize;
        CallBackFn = CallBack;

        if (!CreateMyWindow())
            throw OSException("Failed to create window");

        MainLoop();
    }

    void OSImGui_External::AttachAnotherWindow(std::string DestWindowName, std::string DestWindowClassName, std::function<void()> CallBack)
    {
        Type = ATTACH;
        DestWindow.Name = DestWindowName;
        DestWindow.ClassName = DestWindowClassName;
        CallBackFn = CallBack;

        // Find target window
        DestWindow.hWnd = FindWindowA(DestWindowClassName.empty() ? nullptr : DestWindowClassName.c_str(), DestWindowName.c_str());
        if (!DestWindow.hWnd)
            throw OSException("Failed to find destination window");

        if (!CreateMyWindow())
            throw OSException("Failed to create overlay window");

        MainLoop();
    }

    bool OSImGui_External::CreateMyWindow()
    {
        Window.hInstance = GetModuleHandle(nullptr);

        WNDCLASSEX wc = {};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_CLASSDC;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = Window.hInstance;
        wc.lpszClassName = "OSImGuiOverlay";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

        RegisterClassEx(&wc);

        // Create overlay window with DirectComposition (Chromium-style)
        // WS_EX_NOREDIRECTIONBITMAP + WS_EX_LAYERED: Keep DComp performance + fix click-through
        // WS_EX_LAYERED is REQUIRED for click-through (WS_EX_TRANSPARENT works only with it)
        Window.hWnd = CreateWindowEx(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            wc.lpszClassName,
            Window.Name.c_str(),
            WS_POPUP,
            0, 0, 1920, 1080,
            nullptr,
            nullptr,
            wc.hInstance,
            nullptr
        );

        if (!Window.hWnd)
            return false;

        // Initialize Layered attributes (REQUIRED for click-through hit-testing)
        // Even though we render via DComp, Windows needs this for proper hittest behavior
        SetLayeredWindowAttributes(Window.hWnd, RGB(0, 0, 0), 255, LWA_ALPHA);

        // Make window topmost
        SetWindowPos(Window.hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);

        ShowWindow(Window.hWnd, SW_SHOW);
        UpdateWindow(Window.hWnd);

        // Автоопределение герцовки и установка FPS равной герцовке монитора
        esp::g_menu_config.MonitorRefreshRate = GetMonitorRefreshRate();
        if (esp::g_menu_config.MonitorRefreshRate > 0) {
            esp::g_menu_config.OverlayFPS = esp::g_menu_config.MonitorRefreshRate;
            printf("[OVERLAY] Monitor refresh rate detected: %d Hz, FPS set to %d\n", 
                esp::g_menu_config.MonitorRefreshRate, esp::g_menu_config.OverlayFPS);
        }

        return true;
    }

    bool OSImGui_External::UpdateWindowData()
    {
        if (Type == ATTACH && DestWindow.hWnd)
        {
            // Check if target window still exists
            if (!IsWindow(DestWindow.hWnd))
                return false;

            // Get target window position and size
            RECT rect;
            GetWindowRect(DestWindow.hWnd, &rect);

            // Update overlay window to match
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;

            if (width != Window.Size.x || height != Window.Size.y ||
                rect.left != Window.Pos.x || rect.top != Window.Pos.y)
            {
                Window.Size = Vec2((float)width, (float)height);
                Window.Pos = Vec2((float)rect.left, (float)rect.top);

                SetWindowPos(Window.hWnd, HWND_TOPMOST, rect.left, rect.top, width, height, SWP_NOACTIVATE);
            }
        }

        // Update window transparency based on menu state
        static bool lastMenuState = false;
        bool currentMenuState = esp::g_menu_config.ShowMenu;
        
        // Update Anti-OBS (streamproof) setting
        static bool lastAntiOBSState = false;
        bool currentAntiOBSState = esp::g_menu_config.AntiOBS;
        
        if (currentAntiOBSState != lastAntiOBSState)
        {
            SetWindowDisplayAffinity(Window.hWnd, currentAntiOBSState ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
            lastAntiOBSState = currentAntiOBSState;
        }
        
        if (currentMenuState != lastMenuState)
        {
            LONG_PTR exStyle = GetWindowLongPtr(Window.hWnd, GWL_EXSTYLE);
            
            if (currentMenuState)
            {
                // Menu is open - remove WS_EX_TRANSPARENT and WS_EX_NOACTIVATE to allow clicks
                // Keep WS_EX_NOREDIRECTIONBITMAP (DComp) + WS_EX_LAYERED (hit-testing)
                exStyle &= ~WS_EX_TRANSPARENT;
                exStyle &= ~WS_EX_NOACTIVATE;
                
                SetWindowLongPtr(Window.hWnd, GWL_EXSTYLE, exStyle);
                
                // Force Windows to recalculate hit testing for mouse
                SetWindowPos(Window.hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
                
                // Bring overlay to the front and capture focus
                SetForegroundWindow(Window.hWnd);
                BringWindowToTop(Window.hWnd);
            }
            else
            {
                // Menu is closed - add WS_EX_TRANSPARENT and WS_EX_NOACTIVATE for click-through
                // Keep WS_EX_NOREDIRECTIONBITMAP (DComp) + WS_EX_LAYERED (hit-testing)
                exStyle |= WS_EX_TRANSPARENT;
                exStyle |= WS_EX_NOACTIVATE;
                
                SetWindowLongPtr(Window.hWnd, GWL_EXSTYLE, exStyle);
                
                // Force Windows to recalculate hit testing for mouse
                SetWindowPos(Window.hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
            }
            
            lastMenuState = currentMenuState;
        }

        // Check for exit
        if (PeekEndMessage())
            return false;

        return true;
    }

    bool OSImGui_External::PeekEndMessage()
    {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                return true;
            
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return false;
    }

    void OSImGui_External::MainLoop()
    {
        // Create D3D device
        if (!g_Device.CreateDeviceD3D(Window.hWnd))
            throw OSException("Failed to create D3D device");

        // Initialize ImGui
        if (!InitImGui(g_Device.g_pd3dDevice, g_Device.g_pd3dDeviceContext))
            throw OSException("Failed to initialize ImGui");

        // Show the window
        ShowWindow(Window.hWnd, SW_SHOWDEFAULT);
        UpdateWindow(Window.hWnd);

        // Main loop
        while (!EndFlag)
        {
            // Update window position if attached
            if (!UpdateWindowData())
                break;

            // Start frame
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            // Set background to transparent
            ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;

            // Call user render function
            if (CallBackFn)
                CallBackFn();

            // Render
            ImGui::EndFrame();
            ImGui::Render();

            // For FLIP_DISCARD: Recreate RenderTarget each frame (backbuffer changes after Present)
            g_Device.CleanupRenderTarget();
            g_Device.CreateRenderTarget();

            const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            g_Device.g_pd3dDeviceContext->OMSetRenderTargets(1, &g_Device.g_mainRenderTargetView, nullptr);
            g_Device.g_pd3dDeviceContext->ClearRenderTargetView(g_Device.g_mainRenderTargetView, clear_color_with_alpha);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            // Present with configurable FPS target (VSync OFF to avoid transparent window lock)
            // Soft frame limiter: Sleep-based waiting to avoid CPU burn
            static auto next_frame = std::chrono::high_resolution_clock::now();
            int target_fps = esp::g_menu_config.OverlayFPS;
            if (target_fps < 30) target_fps = 30;
            if (target_fps > 180) target_fps = 180; // Cap at 180 FPS for high refresh monitors
            next_frame += std::chrono::microseconds(1000000 / target_fps);
            
            // VSync OFF (0, 0) - fixes transparent window FPS lock
            g_Device.g_pSwapChain->Present(0, 0);
            
            // Soft wait: Sleep only, no spin-wait to save CPU (~40-50 FPS gain in CS2)
            auto remaining = next_frame - std::chrono::high_resolution_clock::now();
            if (remaining > std::chrono::milliseconds(1))
            {
                std::this_thread::sleep_for(remaining - std::chrono::milliseconds(1));
            }
        }

        // Cleanup
        CleanImGui();
        g_Device.CleanupDeviceD3D();

        // Destroy window
        DestroyWindow(Window.hWnd);
        UnregisterClass("OSImGuiOverlay", Window.hInstance);
    }
}
