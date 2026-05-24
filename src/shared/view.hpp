#pragma once
#include "game_data.hpp"
#include "../sdk/datatypes/vector.hpp"
#include <vector>
// Forward declare ImGui 2D vector to avoid pulling imgui headers into shared API
struct ImVec2;

namespace shared
{
    // World to screen — true only if the point lies inside the viewport.
    bool world_to_screen(
        const vector_t& world_pos,
        vector_t& out_screen_pos,
        const std::array<float, 16>& view_matrix,
        float screen_width,
        float screen_height
    );

    // Always projects when in front of the camera (allows off-screen coordinates).
    bool world_to_screen_project(
        const vector_t& world_pos,
        vector_t& out_screen_pos,
        const std::array<float, 16>& view_matrix,
        float screen_width,
        float screen_height
    );

    // True if the 2D rect overlaps the screen (with optional margin).
    bool screen_rect_intersects_viewport(
        float min_x, float min_y, float max_x, float max_y,
        float screen_width, float screen_height,
        float margin = 48.0f
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

    // Convex hull utilities (used to draw Inferno/molotov area)
    float cross_product(const ImVec2& O, const ImVec2& A, const ImVec2& B);
    std::vector<ImVec2> build_convex_hull(std::vector<ImVec2>& points);
}
