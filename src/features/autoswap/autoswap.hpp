#pragma once
#include <string>

namespace f
{
    namespace autoswap
    {
        bool is_sniper_rifle(const std::string& weapon_name);
        void handle_auto_swap(int current_shots_fired, const std::string& current_weapon, int& last_shots_fired, bool& swap_pending);
    }
}
