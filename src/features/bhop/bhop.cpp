#include "pch.hpp"
#include "bhop.hpp"
#include "../../overlay/menu_config.hpp"
#include "../../sdk/entity.hpp"
#include "../../utils/memory.hpp"
#include "../../common.hpp"
#include <random>
#include <thread>
#include <chrono>

namespace features
{
    namespace bhop
    {
        void handle_bhop(uintptr_t local_pawn)
        {
            if (local_pawn == 0) return;
            if (!esp::g_menu_config.BhopEnabled) return;

            // 1. Читаем ФИЗИЧЕСКИЙ пробел (кнопка активации чита)
            if (!(GetAsyncKeyState(VK_SPACE) & 0x8000)) return;

            // 2. Проверяем, на земле ли игрок
            const auto flags = m_memory->read_t<uint32_t>(local_pawn + g_offsets::m_fFlags);
            bool on_ground = (flags & (1 << 0)) != 0;

            if (!on_ground) return;

            // Кулдаун между прыжками (защита от спама внутри одного тика)
            static auto last_jump = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_jump).count() < 25) return;

            // Рандомизация
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_int_distribution<> chance_dist(0, 99);

            if (chance_dist(gen) < 100) // 100% для тестов
            {
                std::uniform_int_distribution<> delay_dist(1, 5);
                Sleep(delay_dist(gen));

                // 3. ОТПРАВЛЯЕМ АППАРАТНЫЙ КЛИК НА 'P' (0x50)
                INPUT input = { 0 };
                input.type = INPUT_KEYBOARD;
                // Получаем скан-код клавиши P на уровне железа
                input.ki.wScan = MapVirtualKey(0x50, MAPVK_VK_TO_VSC);
                input.ki.time = 0;
                input.ki.dwExtraInfo = 0;
                input.ki.wVk = 0; // Строго 0 при использовании скан-кодов

                // НАЖАТИЕ (DOWN)
                input.ki.dwFlags = KEYEVENTF_SCANCODE;
                SendInput(1, &input, sizeof(INPUT));

                // Удержание (Source 2 требует время на регистрацию клика, 10-15мс)
                std::this_thread::sleep_for(std::chrono::milliseconds(10 + (rand() % 6)));

                // ОТПУСКАНИЕ (UP)
                input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
                SendInput(1, &input, sizeof(INPUT));

                last_jump = std::chrono::steady_clock::now();
            }
        }
    }
}