#pragma once
#include <cstdint>
#include <chrono>
#include <windows.h>  // Для VK_PGDN и DEVMODE 

namespace esp
{
    struct MenuConfig
    {
        // ESP Toggles
        bool ShowBoxESP = true;
        bool ShowSkeleton = true;
        bool ShowHealthBar = true;
        bool ShowName = true;
        bool ShowWeapon = true;
        bool ShowDistance = true;     // <--- ДОБАВИЛИ СЮДА
        bool ShowTeamESP = false;     // <--- Team ESP toggle

        // ESP Colors
        uint32_t BoxColorEnemy = 0xFF0000FF;    // Red (ABGR)
        uint32_t BoxColorTeammate = 0x00FF00FF; // Green (ABGR)
        uint32_t SkeletonColor = 0xFFFFFFFF;    // White (ABGR)

        // Box Style
        int BoxStyle = 0;             // <--- ДОБАВИЛИ СЮДА (0 - Скелет/Углы, 1 - 2D, 2 - Скругленный)
        float BoxThickness = 1.0f;
        bool BoxFill = false;
        uint32_t BoxFillColor = 0x40000000;    // Semi-transparent red

        // Health Bar Style
        bool HealthBarVertical = true;
        float HealthBarWidth = 4.0f;

        // Menu State
        bool ShowMenu = false;
        bool ESPEnabled = true;

        // Overlay FPS Selection (buttons: 60, 90, 120, 180, 240)
        int OverlayFPS = 180;
        
        // Auto-detected monitor refresh rate
        int MonitorRefreshRate = 0;  // 0 = не определено, иначе значение в Гц

        // Triggerbot Settings
        bool TriggerBotEnabled = true;
        bool TriggerFriendlyFire = false; // Стрелять по тиммейтам (для DM)
        int TriggerBotKey = 5;         // 5 = Mouse4 (VK_XBUTTON1) по умолчанию
        int TriggerDelay = 20;         // Базовая задержка в мс (20-300)
        
        // Auto-Swap Settings
        bool AutoSwapEnabled = false;  // Авто-смена оружия для снайперок
        
        // ESP Extrapolation Settings
        float ExtrapolationAmount = 30.0f;  // Миллисекунды экстраполяции для плавности
        
        // Anti-OBS Settings
        bool AntiOBS = false;  // Streamproof mode
        
        // Bhop Settings
        bool BhopEnabled = false;  // Humanized bunny hop
        
        // Kill Sound Settings
        bool KillSoundEnabled = true;  // Play sound on kill
        float KillSoundVolume = 0.5f;  // Volume control (0.0 - 1.0)
        
        // Auto-Pistol Settings
        bool AutoPistolEnabled = false;  // Auto-pistol for pistols (R8 excluded)
        int AutoPistolKey = 2;  // 0=Mouse3, 1=Mouse4, 2=Mouse5, 3=ALT
        
        // Внутренние переменные для таймингов (с сайта не меняются)
        std::chrono::steady_clock::time_point triggerTargetAcquiredTime;
        int currentRandomDelay = 0;    // Вычисленная задержка с учетом рандома
        bool isTargetInFocus = false;  // Флаг: враг уже в прицеле и таймер идет

        // Bomb Window - DISABLED
        // bool ShowBombWindow = true;
    };

    // Global menu config
    inline MenuConfig g_menu_config;
}
