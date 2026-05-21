#pragma once
#include "game_data.hpp"
#include "../sdk/datatypes/vector.hpp"

namespace shared
{
    // World to screen transformation
    // Returns true if point is on screen
    bool world_to_screen(
        const vector_t& world_pos,
        vector_t& out_screen_pos,
        const std::array<float, 16>& view_matrix,
        float screen_width,
        float screen_height
    );
    
    // Calculate 2D box for ESP
    // Returns true if box is valid and on screen
    bool calculate_2d_box(
        const PlayerData& player,
        float& out_x, float& out_y,
        float& out_width, float& out_height,
        const std::array<float, 16>& view_matrix,
        float screen_width,
        float screen_height
    );
    
    // FAST 2-POINT BOX: Projects only 2 points instead of 8 AABB corners
    // 75% reduction in W2S calls per player
    bool calculate_2d_box_fast(
        const PlayerData& player,
        float& out_x, float& out_y,
        float& out_width, float& out_height,
        const std::array<float, 16>& view_matrix,
        float screen_width,
        float screen_height
    );
}
