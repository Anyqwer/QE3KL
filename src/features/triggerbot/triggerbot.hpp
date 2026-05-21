#pragma once
#include "../../shared/game_data.hpp"
#include "../../overlay/menu_config.hpp"
#include <Windows.h>
#include <chrono>
#include <thread>
#include <random>

namespace f::triggerbot
{
    inline void run(const shared::LocalPlayerData& local, const std::vector<shared::PlayerData>& players)
    {
        // Статические переменные хранят состояние между кадрами
        static std::chrono::steady_clock::time_point target_acquired_time;
        static auto last_shot_time = std::chrono::steady_clock::now();
        static bool target_in_crosshair = false;
        static bool logged_crosshair = false;

        // 1. Проверяем, включен ли триггербот в меню
        if (!esp::g_menu_config.TriggerBotEnabled)
            return;

        // 2. Если кнопка НЕ зажата - СБРАСЫВАЕМ таймеры и выходим
        if (!(GetAsyncKeyState(esp::g_menu_config.TriggerBotKey) & 0x8000))
        {
            target_in_crosshair = false;
            logged_crosshair = false;
            return;
        }

        // 3. Защита от спама логов (выводит 1 раз при нажатии)
        if (!logged_crosshair)
        {
            
            logged_crosshair = true;
        }

        // 4. Твоё условие: стреляем по ЛЮБОЙ сущности с ID > 0
        bool entity_in_crosshair = (local.crosshair_id > 0);

        auto now = std::chrono::steady_clock::now();

        if (entity_in_crosshair)
        {
            // Если только что навелись на цель - засекаем время
            if (!target_in_crosshair)
            {
                target_acquired_time = now;
                target_in_crosshair = true;
               
            }

            // Проверяем, прошла ли задержка из меню
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - target_acquired_time).count();

            if (elapsed >= esp::g_menu_config.TriggerDelay)
            {
                // Защита от пулемета (кулдаун 50мс между выстрелами)
                auto time_since_last_shot = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_shot_time).count();

                if (time_since_last_shot >= 50)
                {
                    

                    // Создаем отдельный поток для клика мышью, чтобы ESP не лагал!
                    std::thread([]() {
                        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                        std::this_thread::sleep_for(std::chrono::milliseconds(10 + (rand() % 11))); // Рандомное удержание ЛКМ
                        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
                        }).detach();

                    // Обновляем время выстрела
                    last_shot_time = std::chrono::steady_clock::now();

                    // Сбрасываем флаг фокуса. Если зажимаешь ЛКМ на враге, 
                    // для следующей пули снова отработает задержка Delay.
                    target_in_crosshair = false;
                }
            }
        }
        else
        {
            // Если прицел ушел с цели (смотришь в стену) - сбрасываем таймер наведения
            if (target_in_crosshair)
            {
               
            }
            target_in_crosshair = false;
        }
    }
}