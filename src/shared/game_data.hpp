#pragma once
#include <vector>
#include <string>
#include <array>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <atomic>
#include <algorithm>
#include "../sdk/datatypes/vector.hpp"

using vector_t = f_vector;

namespace shared
{
    // Maximum players in CS2
    constexpr size_t MAX_PLAYERS = 64;
    
    // Player data structure - used by both WebRadar and ImGui ESP
    struct PlayerData
    {
        // Identity
        int index = 0;
        int entity_id = 0;  // pawn entity entry index (m_iIDEntIndex / crosshair)
        std::string name;
        std::string steam_id;
        
        // Team (2 = Terrorists, 3 = Counter-Terrorists)
        int team = 0;
        
        // Player color (for radar/playercard)
        int color = 0;
        
        // Health (0-100)
        int health = 0;
        bool is_dead = false;
        
        // Cached screen position (for optimization)
        vector_t cached_screen_pos;
        uint64_t screen_pos_timestamp = 0;
        
        // Armor and money
        int armor = 0;
        int money = 0;
        bool has_helmet = false;
        bool has_defuser = false;
        bool is_scoped = false;
        
        // Position in world coordinates
        vector_t world_pos;
        
        // Previous position for LERP interpolation (Memory Thread updates this)
        vector_t prev_world_pos;
        
        // Timestamp of last memory update (for calculating LERP alpha)
        uint64_t memory_update_time = 0;
        
        // LERP + Velocity Extrapolation: returns smooth position with prediction
        // Call from Render Thread (high frequency) for smooth ESP
        // Handles lateral movement better than pure LERP
        vector_t get_lerp_position() const
        {
            if (memory_update_time == 0)
                return world_pos; // No previous data yet
            
            // Calculate time since last memory update
            auto now = std::chrono::steady_clock::now();
            auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count() - static_cast<int64_t>(memory_update_time);
            float elapsed_sec = static_cast<float>(elapsed_us) / 1'000'000.0f;
            
            // Normalize: 0 = just updated, 1 = ready for next update
            // Typical frame time: 15600 microseconds (64Hz)
            constexpr int64_t FRAME_TIME_US = 15625; // 1/64 second in microseconds
            float alpha = std::clamp(static_cast<float>(elapsed_us) / FRAME_TIME_US, 0.0f, 1.0f);
            
            // === LERP base position ===
            vector_t lerp_pos = prev_world_pos + (world_pos - prev_world_pos) * alpha;
            
            // === VELOCITY EXTRAPOLATION ===
            // For lateral movement: predict where player WILL be based on velocity
            // This eliminates "lag" for fast strafing (250+ units/sec)
            // Formula: position + velocity * delta_time * extrapolation_factor
            
            // Only extrapolate if we have meaningful velocity
            float vel_mag_sq = velocity.length_sqr();
            if (vel_mag_sq > 10.0f)  // Moving faster than ~5 units/sec
            {
                // Extrapolation factor: 0.3 = conservative (stable), 0.6 = aggressive (risky)
                // 0.4 is sweet spot for CS2 movement
                constexpr float EXTRAPOLATION_FACTOR = 0.4f;
                
                // Predict position: where player will be in 'elapsed_sec' time
                // We add velocity * time to LERP position
                vector_t extrapolation = velocity * elapsed_sec * EXTRAPOLATION_FACTOR;
                
                // Blend based on alpha: more extrapolation mid-frame, less at boundaries
                // At alpha=0 (fresh data): no extrapolation needed
                // At alpha=0.5 (mid-frame): full extrapolation
                // At alpha=1.0 (about to update): reduce to avoid overshoot
                float extrap_weight = std::sin(alpha * 3.14159f);  // Smooth curve: 0 -> 1 -> 0
                
                return lerp_pos + (extrapolation * extrap_weight);
            }
            
            return lerp_pos;
        }
        
        // Get extrapolated bone position for skeleton LOD
        // Applies same extrapolation logic to individual bones
        vector_t get_extrapolated_bone(const vector_t& bone_pos) const
        {
            // Calculate time since last memory update
            auto now = std::chrono::steady_clock::now();
            auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()).count() - static_cast<int64_t>(memory_update_time);
            float elapsed_sec = static_cast<float>(elapsed_us) / 1'000'000.0f;
            
            // Apply same extrapolation as main position
            float vel_mag_sq = velocity.length_sqr();
            if (vel_mag_sq > 10.0f)
            {
                constexpr float EXTRAPOLATION_FACTOR = 0.4f;
                return bone_pos + (velocity * elapsed_sec * EXTRAPOLATION_FACTOR);
            }
            return bone_pos;
        }
        
        // Velocity (for extrapolation)
        vector_t velocity;
        
        // Collision bounds (for 3D box projection)
        vector_t mins;
        vector_t maxs;
        
        // RCS data
        vector_t aim_punch_angle;
        int32_t shots_fired = 0;
        
        // Head position (for ESP skeleton)
        vector_t head_pos;
        
        // Bones (for ESP skeleton) - FIXED ARRAY for cache-friendly access
        // Replaces std::unordered_map (2x memory overhead, pointer chasing)
        static constexpr int MAX_BONES = 32;
        std::array<vector_t, MAX_BONES> bone_positions{};  // Index = bone ID
        uint32_t bone_mask = 0;  // Bit i = 1 if bone i is valid
        
        // Helper: check if bone exists (inline for performance)
        bool has_bone(int idx) const {
            return idx >= 0 && idx < MAX_BONES && (bone_mask & (1u << idx));
        }
        
        // Helper: get bone position (returns zero vector if not present)
        vector_t get_bone(int idx) const {
            if (has_bone(idx)) return bone_positions[idx];
            return vector_t(0, 0, 0);
        }
        
        // Position on screen (for ESP)
        // x, y in screen pixels; z = depth (for visibility check)
        vector_t screen_pos;
        
        // Is player on screen (visible in current view)
        bool is_on_screen = false;
        
        // View angles (for radar direction)
        float yaw = 0.0f;
        float eye_angle = 0.0f;
        
        // Weapons
        struct Weapons
        {
            std::string primary;
            std::string secondary;
            std::vector<std::string> melee;
            std::vector<std::string> utilities;
            std::string active;
        } weapons;
        
        // Weapon name (legacy, for compatibility)
        std::string weapon_name;
        
        // Has C4
        bool has_c4 = false;
        
        // Is alive (legacy, for compatibility)
        bool is_alive = false;
        
        // 2D bounding box for ESP (x, y, width, height)
        // Only valid if is_on_screen is true
        float box_x = 0.0f;
        float box_y = 0.0f;
        float box_width = 0.0f;
        float box_height = 0.0f;
    };
    
    // Bomb data
    struct BombData
    {
        bool is_planted = false;
        bool is_carried = false;
        bool is_dropped = false;
        vector_t position;
        float timer = 0.0f;  // Time until explosion if planted
        
        // Extended bomb data for timer window
        int site = 0;  // 0 = Site A, 1 = Site B
        bool is_defusing = false;
        float defuse_timer = 0.0f;  // Time until defuse complete
        bool can_defuse = false;  // true if CT can defuse in time
    };
    
    // Local player data
    struct LocalPlayerData
    {
        bool is_valid = false;
        int team = 0;
        vector_t position;
        vector_t view_angles;
        float fov = 90.0f;

        // Crosshair ID for triggerbot (m_iIDEntIndex)
        int crosshair_id = 0;

        // View matrix for WorldToScreen
        std::array<float, 16> view_matrix{};

        // Auto-swap data
        int32_t shots_fired = 0;
        std::string weapon_name;
        
        // Bhop data - raw pointer to local pawn entity
        void* local_pawn = nullptr;
    };
    
    // Shared game state - thread-safe access
    class GameState
    {
    public:
        // Thread-safe write (called from memory reading thread)
        void update_players(const std::vector<PlayerData>& players);
        void update_bomb(const BombData& bomb);
        void update_local(const LocalPlayerData& local);
        void update_map(const std::string& map_name);
        void clear();
        
        // Thread-safe read (called from ImGui render thread)
        // Returns a COPY of data, safe to use without lock
        std::vector<PlayerData> get_players() const;
        BombData get_bomb() const;
        LocalPlayerData get_local() const;
        std::string get_map_name() const;
    private:
        mutable std::shared_mutex mutex;
        std::vector<PlayerData> players;
        BombData bomb;
        LocalPlayerData local;
        std::string map_name;
    };
    
    // Double-buffered game state for lock-free reads
    class DoubleBufferedGameState
    {
    public:
        // Write to write buffer (called from memory thread)
        void write_players(const std::vector<PlayerData>& players);
        void write_bomb(const BombData& bomb);
        void write_local(const LocalPlayerData& local);
        void write_map(const std::string& map_name);
        void write_clear();
        
        // Read from read buffer (called from render thread) - NO LOCK
        std::vector<PlayerData> read_players() const;
        BombData read_bomb() const;
        LocalPlayerData read_local() const;
        std::string read_map() const;
        uint64_t read_last_update_time() const;
        // Swap buffers (called after memory update)
        void swap_buffers();
        
    private:
        GameState buffers[2];
        std::atomic<int> write_index{0};
        std::atomic<int> read_index{1};
        std::atomic<uint64_t> last_update_time{0};  // Timestamp for extrapolation
    };
    
    // Helper function for position extrapolation
    inline vector_t extrapolate_position(const vector_t& origin, const vector_t& velocity, float extrapolation_ms)
    {
        // Convert milliseconds to seconds for the formula
        float extrapolation_seconds = extrapolation_ms / 1000.0f;
        
        // Core extrapolation formula: extrapolated_pos = origin + (velocity * time)
        return vector_t(
            origin.m_x + (velocity.m_x * extrapolation_seconds),
            origin.m_y + (velocity.m_y * extrapolation_seconds),
            origin.m_z + (velocity.m_z * extrapolation_seconds)
        );
    }

    // Hitmarker pulse (memory thread -> render thread)
    struct HitmarkerPulse
    {
        bool is_kill = false;
    };

    class HitmarkerBus
    {
    public:
        void push(bool is_kill);
        std::vector<HitmarkerPulse> drain_all();

    private:
        mutable std::mutex mutex;
        std::vector<HitmarkerPulse> queue;
    };

    // Global instances
    inline GameState g_game_state;  // Legacy (for WebSocket)
    inline DoubleBufferedGameState g_double_buffered_state;  // For ImGui ESP
    inline HitmarkerBus g_hitmarker_bus;
}
