#include "pch.hpp"
#include "autoswap.hpp"
#include <Windows.h>
#include <thread>
#include <chrono>
#include <iostream>

namespace f
{
    namespace autoswap
    {
        bool is_sniper_rifle(const std::string& weapon_name) {
            return weapon_name.find("awp") != std::string::npos ||
                   weapon_name.find("ssg08") != std::string::npos;
        }

        void handle_auto_swap(int current_shots_fired, const std::string& current_weapon, int& last_shots_fired, bool& swap_pending) 
        {
            // Обработка перезарядки, начала нового раунда или смерти (счетчик сбрасывается в 0)
            if (current_shots_fired < last_shots_fired) {
                last_shots_fired = current_shots_fired;
                return;
            }

            // Проверяем: это снайперка? Увеличился ли счетчик выстрелов? Свободен ли флаг?
            if (is_sniper_rifle(current_weapon) && current_shots_fired > last_shots_fired && !swap_pending) {
                
                swap_pending = true;
                last_shots_fired = current_shots_fired;
                
                
                // Запускаем макрос в ОТДЕЛЬНОМ ПОТОКЕ, чтобы не фризить чит!
                std::thread([](bool* pending_flag) {
                    
                    // 1. Небольшая задержка перед началом свапа (чтобы выстрел точно засчитался сервером)
                    std::this_thread::sleep_for(std::chrono::milliseconds(30)); 

                    // 2. Достаем НОЖ (Кнопка '3')
                    keybd_event('3', 0, 0, 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(15));
                    keybd_event('3', 0, KEYEVENTF_KEYUP, 0);

                    // 3. Пауза с ножом в руках (CS2 требует небольшую задержку, иначе не засчитает свап)
                    std::this_thread::sleep_for(std::chrono::milliseconds(35));

                    // 4. Достаем ОСНОВНОЕ ОРУЖИЕ обратно (Кнопка '1')
                    keybd_event('1', 0, 0, 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(15));
                    keybd_event('1', 0, KEYEVENTF_KEYUP, 0);

                    // 5. Кулдаун, чтобы избежать багов при зажиме
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));

                    // Снимаем блокировку, разрешаем следующий свап
                    *pending_flag = false;
                    
                    
                }, &swap_pending).detach(); // .detach() позволяет потоку работать независимо
            }
            else if (current_shots_fired > last_shots_fired) {
                // Если мы стреляем из калаша (не снайперка) - просто обновляем счетчик
                last_shots_fired = current_shots_fired;
            }
        }
    }
}
