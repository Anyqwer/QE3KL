#include "pch.hpp"
#include "triggerbot.hpp"
#include "../../overlay/menu_config.hpp"
#include <Windows.h>
#include <random>
#include <chrono>
#include <thread>
#include <atomic>

namespace f
{
    namespace triggerbot
    {
        // === Рабочий поток для безопасных кликов ===
        // Атомарные флаги для общения между основным потоком игры и потоком кликера
        static std::atomic<bool> g_fire_request{ false };
        static std::atomic<bool> g_worker_initialized{ false };

        void click_worker()
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            // CS2 требует удержания кнопки (Hold Time) для регистрации выстрела. 15-25мс - идеально.
            std::uniform_int_distribution<> hold_time(15, 25);

            while (true)
            {
                if (g_fire_request)
                {
                    // 1. НАЖАТИЕ (DOWN) через аппаратный SendInput
                    INPUT input = { 0 };
                    input.type = INPUT_MOUSE;
                    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                    SendInput(1, &input, sizeof(INPUT));

                    // 2. УДЕРЖАНИЕ (Hold)
                    std::this_thread::sleep_for(std::chrono::milliseconds(hold_time(gen)));

                    // 3. ОТПУСКАНИЕ (UP)
                    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
                    SendInput(1, &input, sizeof(INPUT));

                    // Сообщаем главному потоку, что клик завершен
                    g_fire_request = false;
                }

                // Спим 1мс, чтобы пустой цикл не нагружал процессор (0% нагрузки на CPU)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        int calculate_random_delay(int base_delay)
        {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            // Добавляем случайную задержку от 5 до 15 мс к базовой, чтобы не палиться идеальными таймингами
            std::uniform_int_distribution<> random_offset(5, 15);
            return base_delay + random_offset(gen);
        }

        void run(const shared::LocalPlayerData& local, const std::vector<shared::PlayerData>& players)
        {
            auto& config = esp::g_menu_config;

            // Инициализация фонового потока при самом первом вызове функции (Один раз за всю игру)
            if (!g_worker_initialized)
            {
                g_worker_initialized = true;
                std::thread(click_worker).detach();
            }

            // Внутренние статические переменные (хранят состояние между кадрами)
            static auto last_shot_time = std::chrono::steady_clock::now();
            static std::chrono::steady_clock::time_point triggerTargetAcquiredTime;
            static int last_crosshair_id = -1;
            static int currentRandomDelay = 0;
            static bool isTargetInFocus = false;

            if (!config.TriggerBotEnabled) return;

            // Проверка зажатия бинда (кнопки)
            if (!(GetAsyncKeyState(config.TriggerBotKey) & 0x8000)) {
                isTargetInFocus = false;
                last_crosshair_id = -1;
                return;
            }

            int crosshair_id = local.crosshair_id;

            // Если смотрим в небо/стену (нет валидного ID сущности под прицелом)
            if (crosshair_id <= 0) {
                isTargetInFocus = false;
                last_crosshair_id = -1;
                return;
            }

            // === ПРОВЕРКА FRIENDLY FIRE ===
            if (!config.TriggerFriendlyFire) {
                bool is_enemy = false;
                bool target_found = false;

                for (const auto& player : players) {
                    if (player.entity_id == crosshair_id) {
                        target_found = true;
                        if (player.team != local.team) {
                            is_enemy = true;
                        }
                        break;
                    }
                }

                // Если цель найдена и это наш тиммейт — запрещаем стрельбу
                if (target_found && !is_enemy) {
                    isTargetInFocus = false;
                    last_crosshair_id = -1;
                    return;
                }
            }

            // Если прицел перевелся на ДРУГУЮ сущность — сбрасываем таймер наведения
            if (crosshair_id != last_crosshair_id) {
                isTargetInFocus = false;
                last_crosshair_id = crosshair_id;
            }

            auto now = std::chrono::steady_clock::now();

            // Жесткий кулдаун после предыдущего выстрела (защита от случайного спама ЛКМ)
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_shot_time).count() < 30) {
                return;
            }

            // Если фоновый поток прямо сейчас все еще "нажимает" ЛКМ - ждем
            if (g_fire_request) {
                return;
            }

            // Начинаем отсчет задержки (Delay) в момент первого наведения
            if (!isTargetInFocus) {
                currentRandomDelay = calculate_random_delay(config.TriggerDelay);
                triggerTargetAcquiredTime = now;
                isTargetInFocus = true;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - triggerTargetAcquiredTime).count();

            // Если прицел на враге дольше, чем заданная нами задержка — ВРЕМЯ СТРЕЛЯТЬ
            if (elapsed >= currentRandomDelay) {
                last_shot_time = now;

                // Сбрасываем фокус. Это заставит триггербот снова выждать Delay для следующей пули (Tap-fire эффект).
                isTargetInFocus = false;

                // Сигнализируем рабочему потоку нажать на ЛКМ
                g_fire_request = true;
            }
        }
    }
}