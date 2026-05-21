#include "pch.hpp"
#include "updater/updater.hpp"
#include "features/autoswap/autoswap.hpp"
#include "features/autopistol/autopistol.hpp"
#include "features/bhop/bhop.hpp"
#include "overlay/overlay.hpp"
#include "overlay/menu_config.hpp"
#include "shared/game_data.hpp"
#include "shared/json_adapter.hpp"
#include "utils/config.hpp"
#include "core/batch_optimized_cache.hpp"
#include "utils/skCrypter.h"
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

// Resource identifiers
#include "sdk/sounds/resource.h"

// Global config data for WebSocket callback access
config_data_t g_config_data = {};

// Global batch cache instance
extern BatchOptimizedCache g_batch_cache;

// Volume control function for kill sound
void PlaySoundWithVolume(UINT resource_id, float volume) {
    // Clamp volume to valid range
    volume = std::clamp(volume, 0.0f, 1.0f);
    
    // If volume is 0 or very low, don't play
    if (volume < 0.01f) {
        return;
    }
    
    // If volume is maximum, play directly
    if (volume >= 0.99f) {
        PlaySound(MAKEINTRESOURCE(resource_id), GetModuleHandle(NULL), SND_RESOURCE | SND_ASYNC);
        return;
    }
    
    // For other volumes, we need to create a temporary file with adjusted volume
    static std::wstring temp_file_path;
    static bool temp_file_created = false;
    
    if (!temp_file_created) {
        // Get temp directory
        wchar_t temp_dir[MAX_PATH];
        GetTempPathW(MAX_PATH, temp_dir);
        temp_file_path = std::wstring(temp_dir) + L"cs2_killsound_temp.wav";
        temp_file_created = true;
    }
    
    // Load the resource
    HRSRC hResource = FindResource(GetModuleHandle(NULL), MAKEINTRESOURCE(resource_id), "WAVE");
    if (!hResource) return;
    
    HGLOBAL hGlobal = LoadResource(GetModuleHandle(NULL), hResource);
    if (!hGlobal) return;
    
    LPVOID pData = LockResource(hGlobal);
    DWORD size = SizeofResource(GetModuleHandle(NULL), hResource);
    if (!pData || size < 44) return; // 44 bytes minimum for WAV header
    
    // Parse WAV header to get sample data
    std::vector<uint8_t> wav_data((uint8_t*)pData, (uint8_t*)pData + size);
    
    // Adjust volume for 16-bit PCM samples
    if (wav_data.size() > 44) {
        for (size_t i = 44; i < wav_data.size() - 1; i += 2) {
            int16_t sample = *(int16_t*)&wav_data[i];
            sample = static_cast<int16_t>(sample * volume);
            *(int16_t*)&wav_data[i] = sample;
        }
    }
    
    // Write to temporary file
    std::ofstream temp_file(temp_file_path, std::ios::binary);
    if (temp_file.is_open()) {
        temp_file.write(reinterpret_cast<const char*>(wav_data.data()), wav_data.size());
        temp_file.close();
        
        // Play the adjusted file
        PlaySound(std::string(temp_file_path.begin(), temp_file_path.end()).c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }
}

bool main()
{
    // Загружаем оффсеты из JSON файлов
    if (!updater::load_json_offsets())
    {
        LOG_ERROR("failed to load offsets from JSON files");
        return {};
    }

    
    LOG_INFO("version check skipped (temporary)");

    if (!cfg::setup(g_config_data))
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return {};
    }
    LOG_INFO("config system initialization completed");
    
    // Initialize simple auto-pistol (AHK-style)
    f::autopistol::initialize();
    
    // Load ESP settings from config
    esp::g_menu_config.ShowBoxESP = g_config_data.esp_settings.ShowBoxESP;
    esp::g_menu_config.ShowSkeleton = g_config_data.esp_settings.ShowSkeleton;
    esp::g_menu_config.ShowHealthBar = g_config_data.esp_settings.ShowHealthBar;
    esp::g_menu_config.ShowName = g_config_data.esp_settings.ShowName;
    esp::g_menu_config.ShowWeapon = g_config_data.esp_settings.ShowWeapon;
    esp::g_menu_config.ShowTeamESP = g_config_data.esp_settings.ShowTeamESP;
    esp::g_menu_config.BoxColorEnemy = g_config_data.esp_settings.BoxColorEnemy;
    esp::g_menu_config.BoxColorTeammate = g_config_data.esp_settings.BoxColorTeammate;
    esp::g_menu_config.SkeletonColor = g_config_data.esp_settings.SkeletonColor;
    esp::g_menu_config.BoxThickness = g_config_data.esp_settings.BoxThickness;
    esp::g_menu_config.BoxFill = g_config_data.esp_settings.BoxFill;
    esp::g_menu_config.BoxFillColor = g_config_data.esp_settings.BoxFillColor;
    esp::g_menu_config.HealthBarVertical = g_config_data.esp_settings.HealthBarVertical;
    esp::g_menu_config.HealthBarWidth = g_config_data.esp_settings.HealthBarWidth;
    esp::g_menu_config.ESPEnabled = g_config_data.esp_settings.ESPEnabled;
    esp::g_menu_config.AutoSwapEnabled = g_config_data.esp_settings.AutoSwapEnabled;
    esp::g_menu_config.AutoPistolEnabled = g_config_data.esp_settings.AutoPistolEnabled;
    esp::g_menu_config.AutoPistolKey = g_config_data.esp_settings.AutoPistolKey;
    esp::g_menu_config.BhopEnabled = g_config_data.esp_settings.BhopEnabled;
    esp::g_menu_config.ExtrapolationAmount = g_config_data.esp_settings.ExtrapolationAmount;
    
    // Load triggerbot settings from config
    esp::g_menu_config.TriggerBotEnabled = g_config_data.triggerbot_settings.TriggerEnabled;
    esp::g_menu_config.TriggerBotKey = g_config_data.triggerbot_settings.TriggerKey;
    esp::g_menu_config.TriggerDelay = g_config_data.triggerbot_settings.TriggerDelay;
    esp::g_menu_config.TriggerFriendlyFire = g_config_data.triggerbot_settings.TriggerFriendlyFire;
    
    LOG_INFO("ESP and triggerbot settings loaded from config");

    if (!exc::setup())
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return {};
    }
    LOG_INFO("exception handler initialization completed");

    if (!m_memory->setup())
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return {};
    }
    LOG_INFO("memory initialization completed");

    if (!i::setup())
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return {};
    }
    LOG_INFO("interfaces initialization completed");

    LOG_INFO("schema initialization skipped (using direct offsets)");

    WSADATA wsa_data = {};
    const auto wsa_startup = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (wsa_startup != 0)
    {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        return {};
    }
    LOG_INFO("winsock initialization completed");

    // Start ImGui overlay in a separate thread FIRST (to avoid white screen)
    LOG_INFO("starting ImGui overlay thread...");
    InitOverlay();
    
    // Wait for overlay initialization (2 seconds should be enough)
    std::this_thread::sleep_for(std::chrono::seconds(2));

    const auto ipv4_address = utils::get_ipv4_address(g_config_data);
    if (ipv4_address.empty())
        LOG_WARNING("failed to automatically get your ipv4 address!\n                 we will use '%s' from 'config.json'. if the local ip is wrong, please set it", g_config_data.m_local_ip);

    const auto formatted_address = std::format("ws://{}:22006/cs2_webradar", ipv4_address);
    static auto web_socket = easywsclient::WebSocket::from_url(formatted_address);
    if (!web_socket)
    {
        LOG_ERROR("failed to connect to the web socket ('%s')", formatted_address.c_str());
        // Continue anyway - overlay still works without WebRadar
    }
    else
    {
        LOG_INFO("connected to the web socket ('%s')", formatted_address.data());
    }

    auto start = std::chrono::system_clock::now();

    // Rate limiting timers
    auto last_websocket_send = std::chrono::steady_clock::now();
    constexpr auto websocket_interval = std::chrono::milliseconds(40); // 25Hz for WebSocket
    
    // Auto-swap static variables (persist between loop iterations)
    static int32_t last_shots_fired = 0;
    static bool swap_pending = false;
    
    for (;;)
    {
        // === PERFORMANCE MONITORING ===
        static auto frame_start_time = std::chrono::steady_clock::now();
        uint64_t rpm_calls_this_frame = 0;
        
        // 1. Update SDK and collect game data
        sdk::update();
        f::collect_shared_data();
        f::run();
        
        // 2. Update batch optimized cache (RPM batching - major performance improvement)
        g_batch_cache.update();
        
        // 3. Calculate RPM calls for this frame (BATCH OPTIMIZED)
        // Players: ~16 batch calls for 64 players (vs 128 individual calls)
        // Bones: ~40 calls for 5 bones per player (vs 320 individual calls)
        // Total: ~56 RPM calls per frame (vs 448 before batching)
        const auto& batch_stats = g_batch_cache.get_stats();
        rpm_calls_this_frame = batch_stats.rpm_calls_per_frame;
        
        // Performance logging removed for maximum performance

        // 2. Auto-swap logic (after data collection)
        auto local = shared::g_game_state.get_local();
        if (local.is_valid && esp::g_menu_config.AutoSwapEnabled)
        {
            f::autoswap::handle_auto_swap(local.shots_fired, local.weapon_name, last_shots_fired, swap_pending);
        }
        
        // 3. Bhop logic (if enabled)
        if (local.is_valid && esp::g_menu_config.BhopEnabled)
        {
            features::bhop::handle_bhop(reinterpret_cast<uintptr_t>(local.local_pawn));
        }
        
        // 4. Kill Sound logic (if enabled)
        if (local.is_valid && esp::g_menu_config.KillSoundEnabled)
        {
            static int last_kills = 0;
            
            if (sdk::m_local_controller)
            {
                // Read kills from CCSPlayerController ActionTrackingServices -> m_iNumRoundKills
                const auto action_tracking_services = m_memory->read_t<uintptr_t>(reinterpret_cast<uintptr_t>(sdk::m_local_controller) + g_offsets::m_pActionTrackingServices);
                if (action_tracking_services)
                {
                    const int current_kills = m_memory->read_t<int>(action_tracking_services + g_offsets::m_iNumRoundKills);
                    
                    if (current_kills > last_kills)
                    {
                        last_kills = current_kills;
                        // Play embedded sound from resources with volume control
                        PlaySoundWithVolume(KILL_SOUND, esp::g_menu_config.KillSoundVolume);
                    }
                    else if (current_kills < last_kills)
                    {
                        last_kills = current_kills; // Reset on new round
                    }
                }
            }
        }

        // 2. Convert shared data to JSON and send to WebRadar (rate limited 40ms)
        auto now = std::chrono::steady_clock::now();
        if (now - last_websocket_send >= websocket_interval)
        {
            last_websocket_send = now;
            auto local = shared::g_game_state.get_local();
            auto json_data = shared::game_state_to_json(shared::g_game_state, local);
            if (!json_data.is_null())
            {
                if (web_socket) {
                    web_socket->send(json_data.dump());
                }
            }
        }

        // 3. WebSocket poll (чтобы не отключался)
        // 3. WebSocket poll & receive settings (СЛУШАЕМ САЙТ)
        if (web_socket) {
            web_socket->poll();

            // Читаем всё, что прислал сайт
            web_socket->dispatch([](const std::string& message) {
                try {
                    auto j = nlohmann::json::parse(message);

                    // Если это пакет с настройками
                    if (j.value(JSON_KEY("type"), "") == JSON_KEY("config") && j.contains(JSON_KEY("data"))) {
                        auto root_data = j[JSON_KEY("data")];

                        // 1. Применяем ESP настройки
                        if (root_data.contains(JSON_KEY("esp_settings"))) {
                            auto esp_data = root_data[JSON_KEY("esp_settings")];
                            if (esp_data.contains(JSON_KEY("ESPEnabled"))) esp::g_menu_config.ESPEnabled = esp_data[JSON_KEY("ESPEnabled")];
                            if (esp_data.contains(JSON_KEY("ShowBoxESP"))) esp::g_menu_config.ShowBoxESP = esp_data[JSON_KEY("ShowBoxESP")];
                            if (esp_data.contains(JSON_KEY("ShowSkeleton"))) esp::g_menu_config.ShowSkeleton = esp_data[JSON_KEY("ShowSkeleton")];
                            if (esp_data.contains(JSON_KEY("ShowHealthBar"))) esp::g_menu_config.ShowHealthBar = esp_data[JSON_KEY("ShowHealthBar")];
                            if (esp_data.contains(JSON_KEY("ShowName"))) esp::g_menu_config.ShowName = esp_data[JSON_KEY("ShowName")];
                            if (esp_data.contains(JSON_KEY("ShowWeapon"))) esp::g_menu_config.ShowWeapon = esp_data[JSON_KEY("ShowWeapon")];
                            if (esp_data.contains(JSON_KEY("ShowTeamESP"))) esp::g_menu_config.ShowTeamESP = esp_data[JSON_KEY("ShowTeamESP")];
                            if (esp_data.contains(JSON_KEY("ShowDistance"))) esp::g_menu_config.ShowDistance = esp_data[JSON_KEY("ShowDistance")];
                            if (esp_data.contains(JSON_KEY("BoxStyle"))) esp::g_menu_config.BoxStyle = esp_data[JSON_KEY("BoxStyle")];
                            if (esp_data.contains(JSON_KEY("HealthBarWidth"))) esp::g_menu_config.HealthBarWidth = esp_data[JSON_KEY("HealthBarWidth")];
                            if (esp_data.contains(JSON_KEY("BoxFill"))) esp::g_menu_config.BoxFill = esp_data[JSON_KEY("BoxFill")];
                            if (esp_data.contains(JSON_KEY("HealthBarVertical"))) esp::g_menu_config.HealthBarVertical = esp_data[JSON_KEY("HealthBarVertical")];
                            if (esp_data.contains(JSON_KEY("BoxColorEnemy"))) esp::g_menu_config.BoxColorEnemy = esp_data[JSON_KEY("BoxColorEnemy")];
                            if (esp_data.contains(JSON_KEY("BoxColorTeammate"))) esp::g_menu_config.BoxColorTeammate = esp_data[JSON_KEY("BoxColorTeammate")];
                            if (esp_data.contains(JSON_KEY("SkeletonColor"))) esp::g_menu_config.SkeletonColor = esp_data[JSON_KEY("SkeletonColor")];
                            if (esp_data.contains(JSON_KEY("AutoSwapEnabled"))) esp::g_menu_config.AutoSwapEnabled = esp_data[JSON_KEY("AutoSwapEnabled")];
                            if (esp_data.contains(JSON_KEY("ExtrapolationAmount"))) esp::g_menu_config.ExtrapolationAmount = esp_data[JSON_KEY("ExtrapolationAmount")];
                            if (esp_data.contains(JSON_KEY("AntiOBS"))) esp::g_menu_config.AntiOBS = esp_data[JSON_KEY("AntiOBS")];
                            if (esp_data.contains(JSON_KEY("BunnyHopEnabled"))) esp::g_menu_config.BhopEnabled = esp_data[JSON_KEY("BunnyHopEnabled")];
                            if (esp_data.contains(JSON_KEY("KillSoundEnabled"))) esp::g_menu_config.KillSoundEnabled = esp_data[JSON_KEY("KillSoundEnabled")];
                            if (esp_data.contains(JSON_KEY("KillSoundVolume"))) esp::g_menu_config.KillSoundVolume = esp_data[JSON_KEY("KillSoundVolume")];
                            if (esp_data.contains(JSON_KEY("AutoPistolKey"))) esp::g_menu_config.AutoPistolKey = esp_data[JSON_KEY("AutoPistolKey")];
                            if (esp_data.contains(JSON_KEY("AutoPistolEnabled"))) esp::g_menu_config.AutoPistolEnabled = esp_data[JSON_KEY("AutoPistolEnabled")];
                        }

                        // 2. Применяем Triggerbot настройки (они лежат ВНЕ esp_settings)
                        if (root_data.contains(JSON_KEY("triggerbot"))) {
                            auto trigger_data = root_data[JSON_KEY("triggerbot")];

                            if (trigger_data.contains(JSON_KEY("TriggerEnabled")))
                                esp::g_menu_config.TriggerBotEnabled = trigger_data[JSON_KEY("TriggerEnabled")];

                            if (trigger_data.contains(JSON_KEY("TriggerKey")))
                                esp::g_menu_config.TriggerBotKey = trigger_data[JSON_KEY("TriggerKey")];

                            if (trigger_data.contains(JSON_KEY("TriggerDelay"))) {
                                int newDelay = trigger_data[JSON_KEY("TriggerDelay")];
                                if (newDelay < 0) newDelay = 0;
                                if (newDelay > 500) newDelay = 500;
                                esp::g_menu_config.TriggerDelay = newDelay;
                            }

                            printf("[WebSocket] TriggerBot updated: Enabled=%d, Key=%d, Delay=%d\n",
                                esp::g_menu_config.TriggerBotEnabled,
                                esp::g_menu_config.TriggerBotKey,
                                esp::g_menu_config.TriggerDelay);
                        }

                        printf("[WebSocket] Settings successfully applied!\n");

                        // Auto-save config to disk (preserve IP settings from global config)
                        g_config_data.esp_settings.ShowBoxESP = esp::g_menu_config.ShowBoxESP;
                        g_config_data.esp_settings.ShowSkeleton = esp::g_menu_config.ShowSkeleton;
                        g_config_data.esp_settings.ShowHealthBar = esp::g_menu_config.ShowHealthBar;
                        g_config_data.esp_settings.ShowName = esp::g_menu_config.ShowName;
                        g_config_data.esp_settings.ShowWeapon = esp::g_menu_config.ShowWeapon;
                        g_config_data.esp_settings.ShowTeamESP = esp::g_menu_config.ShowTeamESP;
                        g_config_data.esp_settings.BoxColorEnemy = esp::g_menu_config.BoxColorEnemy;
                        g_config_data.esp_settings.BoxColorTeammate = esp::g_menu_config.BoxColorTeammate;
                        g_config_data.esp_settings.SkeletonColor = esp::g_menu_config.SkeletonColor;
                        g_config_data.esp_settings.BoxThickness = esp::g_menu_config.BoxThickness;
                        g_config_data.esp_settings.BoxFill = esp::g_menu_config.BoxFill;
                        g_config_data.esp_settings.BoxFillColor = esp::g_menu_config.BoxFillColor;
                        g_config_data.esp_settings.HealthBarVertical = esp::g_menu_config.HealthBarVertical;
                        g_config_data.esp_settings.HealthBarWidth = esp::g_menu_config.HealthBarWidth;
                        g_config_data.esp_settings.ESPEnabled = esp::g_menu_config.ESPEnabled;
                        g_config_data.esp_settings.AutoSwapEnabled = esp::g_menu_config.AutoSwapEnabled;
                        g_config_data.esp_settings.ExtrapolationAmount = esp::g_menu_config.ExtrapolationAmount;
                        g_config_data.esp_settings.AntiOBS = esp::g_menu_config.AntiOBS;
                        g_config_data.esp_settings.BhopEnabled = esp::g_menu_config.BhopEnabled;
                        g_config_data.esp_settings.KillSoundEnabled = esp::g_menu_config.KillSoundEnabled;
                        g_config_data.esp_settings.KillSoundVolume = esp::g_menu_config.KillSoundVolume;
                        g_config_data.esp_settings.AutoPistolEnabled = esp::g_menu_config.AutoPistolEnabled;
                        g_config_data.esp_settings.AutoPistolKey = esp::g_menu_config.AutoPistolKey;

                        // Сохраняем настройки триггербота
                        g_config_data.triggerbot_settings.TriggerEnabled = esp::g_menu_config.TriggerBotEnabled;
                        g_config_data.triggerbot_settings.TriggerKey = esp::g_menu_config.TriggerBotKey;
                        g_config_data.triggerbot_settings.TriggerDelay = esp::g_menu_config.TriggerDelay;
                        g_config_data.triggerbot_settings.TriggerFriendlyFire = esp::g_menu_config.TriggerFriendlyFire;

                        if (cfg::save(g_config_data)) {
                            printf("[info] Config auto-saved to disk after web update\n");
                        } else {
                            printf("[error] Failed to auto-save config to disk\n");
                        }
                    }
                }
                catch (...) {
                    // Если пришел битый JSON - просто игнорируем, чтобы чит не крашнулся
                }
                });
        }

        // 4. Ждем 14 миллисекунд (~64Hz, CS2 tick rate)
        std::this_thread::sleep_for(std::chrono::milliseconds(14));

    } // <-- Эта скобка закрывает бесконечный цикл for(;;)

    system("pause");
    return true;

} // <-- Эта скобка закрывает функцию main()