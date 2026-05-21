#include "pch.hpp"
#include "view.hpp"

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
        
        if (!world_to_screen(bottom_pos, screen_bottom, view_matrix, screen_width, screen_height) ||
            !world_to_screen(top_pos, screen_top, view_matrix, screen_width, screen_height))
        {
            return false; // Player off-screen
        }
        
        // Calculate 2D box dimensions
        float height = screen_bottom.m_y - screen_top.m_y;
        
        // Sanity check: height should be positive and reasonable
        if (height < 5.0f || height > screen_height * 0.8f) {
            return false; // Invalid projection
        }
        
        float width = height * PLAYER_ASPECT_RATIO;
        
        // Center the box horizontally
        out_x = screen_top.m_x - (width * 0.5f);
        out_y = screen_top.m_y;
        out_width = width;
        out_height = height;
        
        // Additional check: box should be at least partially on screen
        if (out_x + out_width < 0 || out_x > screen_width ||
            out_y + out_height < 0 || out_y > screen_height)
        {
            return false;
        }
        
        return true;
    }

    bool world_to_screen(
        const vector_t& world_pos,
        vector_t& out_screen_pos,
        const std::array<float, 16>& view_matrix,
        float screen_width,
        float screen_height
    )
    {
        // Matrix multiplication
        float x = view_matrix[0] * world_pos.m_x + view_matrix[1] * world_pos.m_y + view_matrix[2] * world_pos.m_z + view_matrix[3];
        float y = view_matrix[4] * world_pos.m_x + view_matrix[5] * world_pos.m_y + view_matrix[6] * world_pos.m_z + view_matrix[7];
        float z = view_matrix[8] * world_pos.m_x + view_matrix[9] * world_pos.m_y + view_matrix[10] * world_pos.m_z + view_matrix[11];
        float w = view_matrix[12] * world_pos.m_x + view_matrix[13] * world_pos.m_y + view_matrix[14] * world_pos.m_z + view_matrix[15];
        
        // Behind camera
        if (w < 0.001f)
            return false;
        
        // Perspective divide
        float inv_w = 1.0f / w;
        x *= inv_w;
        y *= inv_w;
        
        // Convert to screen coordinates
        out_screen_pos.m_x = (screen_width * 0.5f) + (x * screen_width * 0.5f);
        out_screen_pos.m_y = (screen_height * 0.5f) - (y * screen_height * 0.5f);
        out_screen_pos.m_z = z;  // Depth
        
        // Check if on screen
        if (out_screen_pos.m_x < 0 || out_screen_pos.m_x > screen_width)
            return false;
        if (out_screen_pos.m_y < 0 || out_screen_pos.m_y > screen_height)
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
        
        for (int i = 0; i < 8; i++)
        {
            if (world_to_screen(corners[i], screen_corners[i], view_matrix, screen_width, screen_height))
            {
                visible_corners++;
            }
        }
        
        // If no corners are visible, box is off-screen
        if (visible_corners == 0)
            return false;
        
        // Find min/max X and Y among visible corners
        float min_screen_x = screen_width;
        float max_screen_x = 0.0f;
        float min_screen_y = screen_height;
        float max_screen_y = 0.0f;
        
        for (int i = 0; i < 8; i++)
        {
            if (screen_corners[i].m_x > 0 && screen_corners[i].m_x < screen_width &&
                screen_corners[i].m_y > 0 && screen_corners[i].m_y < screen_height)
            {
                min_screen_x = std::min(min_screen_x, screen_corners[i].m_x);
                max_screen_x = std::max(max_screen_x, screen_corners[i].m_x);
                min_screen_y = std::min(min_screen_y, screen_corners[i].m_y);
                max_screen_y = std::max(max_screen_y, screen_corners[i].m_y);
            }
        }
        
        // If still no valid corners after filtering, use first visible
        if (min_screen_x == screen_width)
        {
            for (int i = 0; i < 8; i++)
            {
                if (screen_corners[i].m_x > 0)
                {
                    min_screen_x = std::min(min_screen_x, screen_corners[i].m_x);
                    max_screen_x = std::max(max_screen_x, screen_corners[i].m_x);
                    min_screen_y = std::min(min_screen_y, screen_corners[i].m_y);
                    max_screen_y = std::max(max_screen_y, screen_corners[i].m_y);
                }
            }
        }
        
        out_x = min_screen_x;
        out_y = min_screen_y;
        out_width = max_screen_x - min_screen_x;
        out_height = max_screen_y - min_screen_y;
        
        return true;
    }
}
