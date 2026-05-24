#include "pch.hpp"
#include "view.hpp"
#include <algorithm>
#include <cfloat>
#include "../tkazer_base/CS2_External/OS-ImGui/imgui/imgui.h"


namespace shared
{
    // FAST 2-POINT BOX: Projects only 2 points instead of 8 AABB corners
    // 75% reduction in W2S calls per player
    bool calculate_2d_box_fast(
        const PlayerData& player,
        float& out_x, float& out_y,
        float& out_width, float& out_height,
        const std::array<float, 16>& view_matrix,
        float screen_width,
        float screen_height
    )
    {
        // CS2 player hitbox: height ~72 units, width ~32 units (aspect ratio ~0.44)
        // But for visual 2D box we use ~0.6 aspect ratio for better aesthetics
        constexpr float PLAYER_ASPECT_RATIO = 0.6f;
        constexpr float HEIGHT_OFFSET_BOTTOM = 0.0f;   // Feet
        constexpr float HEIGHT_OFFSET_TOP = 72.0f;     // Head (~72 units tall)
        
        // Use player's actual collision bounds if available
        float height_min = player.mins.m_z;
        float height_max = player.maxs.m_z;
        
        // Fallback to default if bounds not set
        if (height_max <= height_min) {
            height_min = HEIGHT_OFFSET_BOTTOM;
            height_max = HEIGHT_OFFSET_TOP;
        }
        
        // Project bottom-center (feet)
        vector_t bottom_pos(
            player.world_pos.m_x,
            player.world_pos.m_y,
            player.world_pos.m_z + height_min
        );
        
        // Project top-center (head)
        vector_t top_pos(
            player.world_pos.m_x,
            player.world_pos.m_y,
            player.world_pos.m_z + height_max
        );
        
        vector_t screen_bottom, screen_top;
        const bool feet_ok = world_to_screen_project(bottom_pos, screen_bottom, view_matrix, screen_width, screen_height);
        const bool head_ok = world_to_screen_project(top_pos, screen_top, view_matrix, screen_width, screen_height);

        if (!feet_ok && !head_ok)
            return false;

        if (!feet_ok)
            screen_bottom = screen_top;
        if (!head_ok)
            screen_top = screen_bottom;

        float height = screen_bottom.m_y - screen_top.m_y;
        if (height < 0.0f)
            height = -height;

        if (height < 5.0f || height > screen_height * 0.95f)
            return false;

        float width = height * PLAYER_ASPECT_RATIO;
        out_x = screen_top.m_x - (width * 0.5f);
        out_y = screen_top.m_y;
        out_width = width;
        out_height = height;

        return screen_rect_intersects_viewport(
            out_x, out_y, out_x + out_width, out_y + out_height,
            screen_width, screen_height);
    }

    float cross_product(const ImVec2& O, const ImVec2& A, const ImVec2& B) {
        return (A.x - O.x) * (B.y - O.y) - (A.y - O.y) * (B.x - O.x);
    }

    std::vector<ImVec2> build_convex_hull(std::vector<ImVec2>& points) {
        size_t n = points.size(), k = 0;
        if (n <= 3) return points;

        std::vector<ImVec2> hull(2 * n);

        std::sort(points.begin(), points.end(), [](const ImVec2& a, const ImVec2& b) {
            return a.x < b.x || (a.x == b.x && a.y < b.y);
        });

        // lower
        for (size_t i = 0; i < n; ++i) {
            while (k >= 2 && cross_product(hull[k - 2], hull[k - 1], points[i]) <= 0) k--;
            hull[k++] = points[i];
        }

        // upper
        for (size_t i = n - 1, t = k + 1; i > 0; --i) {
            while (k >= t && cross_product(hull[k - 2], hull[k - 1], points[i - 1]) <= 0) k--;
            hull[k++] = points[i - 1];
        }

        hull.resize(k - 1);
        return hull;
    }

    bool world_to_screen_project(
        const vector_t& world_pos,
        vector_t& out_screen_pos,
        const std::array<float, 16>& view_matrix,
        float screen_width,
        float screen_height
    )
    {
        float x = view_matrix[0] * world_pos.m_x + view_matrix[1] * world_pos.m_y + view_matrix[2] * world_pos.m_z + view_matrix[3];
        float y = view_matrix[4] * world_pos.m_x + view_matrix[5] * world_pos.m_y + view_matrix[6] * world_pos.m_z + view_matrix[7];
        float z = view_matrix[8] * world_pos.m_x + view_matrix[9] * world_pos.m_y + view_matrix[10] * world_pos.m_z + view_matrix[11];
        float w = view_matrix[12] * world_pos.m_x + view_matrix[13] * world_pos.m_y + view_matrix[14] * world_pos.m_z + view_matrix[15];

        if (w < 0.001f)
            return false;

        const float inv_w = 1.0f / w;
        x *= inv_w;
        y *= inv_w;

        out_screen_pos.m_x = (screen_width * 0.5f) + (x * screen_width * 0.5f);
        out_screen_pos.m_y = (screen_height * 0.5f) - (y * screen_height * 0.5f);
        out_screen_pos.m_z = w;
        return true;
    }

    bool screen_rect_intersects_viewport(
        float min_x, float min_y, float max_x, float max_y,
        float screen_width, float screen_height,
        float margin)
    {
        if (max_x < min_x || max_y < min_y)
            return false;

        return !(max_x < -margin ||
            min_x > screen_width + margin ||
            max_y < -margin ||
            min_y > screen_height + margin);
    }

    bool world_to_screen(
        const vector_t& world_pos,
        vector_t& out_screen_pos,
        const std::array<float, 16>& view_matrix,
        float screen_width,
        float screen_height
    )
    {
        if (!world_to_screen_project(world_pos, out_screen_pos, view_matrix, screen_width, screen_height))
            return false;

        if (out_screen_pos.m_x < 0.0f || out_screen_pos.m_x > screen_width)
            return false;
        if (out_screen_pos.m_y < 0.0f || out_screen_pos.m_y > screen_height)
            return false;

        return true;
    }
    
    bool calculate_2d_box(
        const PlayerData& player,
        float& out_x, float& out_y,
        float& out_width, float& out_height,
        const std::array<float, 16>& view_matrix,
        float screen_width,
        float screen_height
    )
    {
        // Project all 8 corners of the AABB (3D hitbox) to screen space
        // Then find the bounding box of the projected points
        
        const float min_x = player.mins.m_x;
        const float min_y = player.mins.m_y;
        const float min_z = player.mins.m_z;
        const float max_x = player.maxs.m_x;
        const float max_y = player.maxs.m_y;
        const float max_z = player.maxs.m_z;
        
        // 8 corners of the AABB (offset by world position)
        vector_t corners[8] = {
            {player.world_pos.m_x + min_x, player.world_pos.m_y + min_y, player.world_pos.m_z + min_z},
            {player.world_pos.m_x + max_x, player.world_pos.m_y + min_y, player.world_pos.m_z + min_z},
            {player.world_pos.m_x + min_x, player.world_pos.m_y + max_y, player.world_pos.m_z + min_z},
            {player.world_pos.m_x + max_x, player.world_pos.m_y + max_y, player.world_pos.m_z + min_z},
            {player.world_pos.m_x + min_x, player.world_pos.m_y + min_y, player.world_pos.m_z + max_z},
            {player.world_pos.m_x + max_x, player.world_pos.m_y + min_y, player.world_pos.m_z + max_z},
            {player.world_pos.m_x + min_x, player.world_pos.m_y + max_y, player.world_pos.m_z + max_z},
            {player.world_pos.m_x + max_x, player.world_pos.m_y + max_y, player.world_pos.m_z + max_z}
        };
        
        // Project all corners to screen space
        vector_t screen_corners[8];
        int visible_corners = 0;
        
        float min_screen_x = FLT_MAX;
        float max_screen_x = -FLT_MAX;
        float min_screen_y = FLT_MAX;
        float max_screen_y = -FLT_MAX;

        for (int i = 0; i < 8; i++)
        {
            if (!world_to_screen_project(corners[i], screen_corners[i], view_matrix, screen_width, screen_height))
                continue;

            visible_corners++;
            min_screen_x = std::min(min_screen_x, screen_corners[i].m_x);
            max_screen_x = std::max(max_screen_x, screen_corners[i].m_x);
            min_screen_y = std::min(min_screen_y, screen_corners[i].m_y);
            max_screen_y = std::max(max_screen_y, screen_corners[i].m_y);
        }

        if (visible_corners == 0)
            return false;

        out_x = min_screen_x;
        out_y = min_screen_y;
        out_width = max_screen_x - min_screen_x;
        out_height = max_screen_y - min_screen_y;

        return screen_rect_intersects_viewport(
            out_x, out_y, out_x + out_width, out_y + out_height,
            screen_width, screen_height);
    }
}
