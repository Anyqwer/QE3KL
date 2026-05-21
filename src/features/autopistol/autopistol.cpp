#include "pch.hpp"
#include "autopistol.hpp"
#include "../../overlay/menu_config.hpp"
#include <Windows.h>
#include <thread>
#include <chrono>
#include <random>
#include <concurrent_queue.h>

namespace f
{
    namespace autopistol
    {
        // Raw Input based auto-pistol with humanization
        struct PistolProfile {
            float base_delay_ms;      // Базовая задержка между кликами
            float variance_percent;   // Процент вариативности
            bool exclude;             // Исключить из автопистолета
        };

        // Профили пистолетов (Deagle и R8 исключены)
        static const std::unordered_map<std::string, PistolProfile> pistol_profiles = {
            {"glock",     {65.0f, 15.0f, false}},    // ~15.4 CPS
            {"usp",       {60.0f, 12.0f, false}},    // ~16.7 CPS  
            {"p250",      {70.0f, 10.0f, false}},    // ~14.3 CPS
            {"beretta",   {55.0f, 18.0f, false}},    // ~18.2 CPS
            {"tec9",      {80.0f, 20.0f, false}},    // ~12.5 CPS
            {"cz75",      {85.0f, 8.0f, false}},     // ~11.8 CPS
            {"fiveseven", {75.0f, 12.0f, false}},    // ~13.3 CPS
            {"deagle",    {0.0f, 0.0f, true}},       // ИСКЛЮЧЕН
            {"revolver",  {0.0f, 0.0f, true}}        // ИСКЛЮЧЕН
        };

        // Состояние автопистолета
        static std::atomic<bool> should_stop{false};
        static std::thread auto_pistol_thread;

        // Humanization engine
        class HumanizationEngine {
        private:
            std::mt19937 rng{std::random_device{}()};
            std::normal_distribution<float> timing_dist{0.0f, 1.0f};
            std::uniform_int_distribution<int> mistake_dist{0, 999};
            
        public:
            // Шанс на ошибку (0.1% - 1%)
            bool ShouldMakeMistake() {
                return mistake_dist(rng) < 5;  // 0.5% шанс на ошибку
            }
            
            // Естественная вариативность таймингов
            float GetHumanizedDelay(float base_delay, float variance_percent) {
                float variance_range = base_delay * (variance_percent / 100.0f);
                std::normal_distribution<float> delay_dist(base_delay, variance_range / 3.0f);
                
                float delay = delay_dist(rng);
                return std::clamp(delay, base_delay * 0.7f, base_delay * 1.3f);
            }
            
            // Случайная пауза для имитации человеческого поведения
            int GetRandomPause() {
                std::uniform_int_distribution<int> pause_dist(0, 99);
                int chance = pause_dist(rng);
                
                if (chance < 2) return 150;      // 2% шанс на длинную паузу
                if (chance < 8) return 80;       // 6% шанс на среднюю паузу
                if (chance < 20) return 30;      // 12% шанс на короткую паузу
                return 0;                        // 80% без паузы
            }
        };

        static HumanizationEngine humanizer;

        // Получение нужной кнопки для автопистолета
        bool IsAutoPistolKeyPressed() {
            switch (esp::g_menu_config.AutoPistolKey) {
                case 0: return (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;  // Mouse 3
                case 1: return (GetAsyncKeyState(VK_XBUTTON1) & 0x8000) != 0; // Mouse 4
                case 2: return (GetAsyncKeyState(VK_XBUTTON2) & 0x8000) != 0; // Mouse 5
                case 3: return (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;      // ALT
                default: return (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0; // Default Mouse 3
            }
        }

        // Определение типа пистолета по имени
        std::string GetPistolType(const std::string& weapon_name) {
            std::string lower_name = weapon_name;
            std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
            
            for (const auto& [name, profile] : pistol_profiles) {
                if (lower_name.find(name) != std::string::npos) {
                    return name;
                }
            }
            return "";
        }

        // Быстрая отправка клика через SendInput
        void SendFastClick() {
            INPUT input = {};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
            SendInput(1, &input, sizeof(INPUT));
            
            // Минимальная задержка между down/up
            Sleep(2);
            
            input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
            SendInput(1, &input, sizeof(INPUT));
        }

        // Поток автопистолета - Pulsing Method для Source 2
        void AutoPistolWorker() {
            auto last_shot_time = std::chrono::steady_clock::now();
            std::mt19937 gen(std::random_device{}());

            while (!should_stop) {
                // 1. Проверка условий (Включен ли в меню и зажата ли настроенная кнопка)
                bool is_key_pressed = IsAutoPistolKeyPressed();
                
                if (!is_key_pressed || !esp::g_menu_config.AutoPistolEnabled) {
                    static int log_counter = 0;
                    if (++log_counter % 1000 == 0) { // Логируем каждые 1000 итераций
                        printf("[AutoPistol] Waiting - Key pressed: %s, Enabled: %s\n", 
                               is_key_pressed ? "true" : "false", 
                               esp::g_menu_config.AutoPistolEnabled ? "true" : "false");
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }

                // 2. Валидация игрока и оружия
                auto local = shared::g_game_state.get_local();
                if (!local.is_valid) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                std::string pistol_type = GetPistolType(local.weapon_name);
                if (pistol_type.empty() || pistol_profiles.at(pistol_type).exclude) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                // 3. ЛОГИКА ВЫСТРЕЛА (Pulsing Method)
                // Нам нужно "обмануть" физический зажим ЛКМ
                
                // А) ПРИНУДИТЕЛЬНЫЙ UP: Заставляем игру поверить, что палец поднят
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                
                // Пауза между UP и DOWN (критично для регистрации сервером/движком)
                // 20-30 мс - это примерно 2-3 игровых тика (64 tick)
                std::this_thread::sleep_for(std::chrono::milliseconds(20 + (gen() % 10)));

                // Б) ПРИНУДИТЕЛЬНЫЙ DOWN: Делаем выстрел
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);

                // В) HOLD TIME: Держим кнопку нажатой, иначе Source 2 проигнорирует клик
                // В CS2 клик длиной 0мс часто не регается
                std::this_thread::sleep_for(std::chrono::milliseconds(25 + (gen() % 10)));

                // Г) ФИНАЛЬНЫЙ UP: Завершаем цикл клика
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);

                // Д) ЗАДЕРЖКА ПЕРЕД СЛЕДУЮЩИМ ЦИКЛОМ (CPS Control)
                // Подбираем под скорострельность конкретного пистолета
                float base_delay = pistol_profiles.at(pistol_type).base_delay_ms;
                float variance = pistol_profiles.at(pistol_type).variance_percent;
                
                int final_sleep = static_cast<int>(humanizer.GetHumanizedDelay(base_delay, variance));
                
                // Вычитаем уже затраченное время (hold time и release time)
                final_sleep = std::max(10, final_sleep - 50); 

                printf("[AutoPistol] Shot fired - Weapon: %s, Delay: %dms, Key: %d\n", 
                       pistol_type.c_str(), final_sleep, esp::g_menu_config.AutoPistolKey);
                
                std::this_thread::sleep_for(std::chrono::milliseconds(final_sleep));
            }
            
            printf("[DEBUG] Source 2 Optimized AutoPistol Stopped\n");
        }

        // Простая инициализация без хуков
        void initialize() {
            // Запуск потока автопистолета
            should_stop = false;
            printf("[AutoPistol] Initializing with key: %d (0=Mouse3, 1=Mouse4, 2=Mouse5, 3=ALT)\n", esp::g_menu_config.AutoPistolKey);
            printf("[AutoPistol] Enabled: %s\n", esp::g_menu_config.AutoPistolEnabled ? "true" : "false");
            std::thread(AutoPistolWorker).detach();
        }

        // Очистка
        void cleanup() {
            should_stop = true;
            
            if (auto_pistol_thread.joinable()) {
                auto_pistol_thread.join();
            }
            
            printf("[INFO] Simple auto-pistol cleaned up\n");
        }
    }
}