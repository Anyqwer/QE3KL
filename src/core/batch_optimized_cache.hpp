#pragma once
#include "batch_structures.hpp"
#include "../sdk/entity.hpp"
#include <vector>
#include <array>
#include <chrono>
#include <bitset>

// === Batch Optimized Cache for Maximum RPM Reduction ===
// Replaces SimpleOptimizedCache with batch reading capabilities
class BatchOptimizedCache {
public:
    BatchOptimizedCache();
    ~BatchOptimizedCache() = default;
    
    // Main update function - reads all entities in batch operations
    void update();
    
    // Get cached player data
    const PlayerSnapshot* get_player_snapshot(int player_index) const;
    const PlayerBoneData* get_player_bones(int player_index) const;
    
    // Get cached weapon data
    const WeaponSnapshot* get_weapon_snapshot(int weapon_index) const;
    
    // Get valid entity indices
    std::vector<int> get_valid_player_indices() const;
    std::vector<int> get_valid_weapon_indices() const;
    
    // Performance statistics
    struct CacheStats {
        size_t rpm_calls_per_frame = 0;
        size_t players_cached = 0;
        size_t bones_cached = 0;
        size_t weapons_cached = 0;
        double avg_batch_time_ms = 0.0;
        size_t cache_hits = 0;
        size_t cache_misses = 0;
        double cache_hit_rate = 0.0;
    };
    
    const CacheStats& get_stats() const { return m_stats; }
    void reset_stats() { m_stats = {}; }
    
    // Cache configuration
    struct CacheConfig {
        std::chrono::milliseconds player_update_interval = std::chrono::milliseconds(16);  // ~60 FPS
        std::chrono::milliseconds bone_update_interval = std::chrono::milliseconds(50);     // ~20 FPS
        std::chrono::milliseconds weapon_update_interval = std::chrono::milliseconds(33);   // ~30 FPS
        std::chrono::milliseconds cache_ttl = std::chrono::milliseconds(100);                // 100ms TTL
        size_t max_players = 64;
        size_t max_weapons = 460;  // 65-524 range
        bool enable_differential_updates = false;
        bool enable_bone_caching = true;
    };
    
    void set_config(const CacheConfig& config) { m_config = config; }
    const CacheConfig& get_config() const { return m_config; }
    
private:
    // === Core Cache Data ===
    std::array<PlayerSnapshot, 64> m_player_snapshots;
    std::array<PlayerBoneData, 64> m_player_bones;
    std::array<uint64_t, 64> m_player_timestamps;
    std::bitset<64> m_valid_players;
    std::bitset<64> m_updated_players;
    
    // Weapon cache data
    std::array<WeaponSnapshot, 460> m_weapon_snapshots;
    std::array<uint64_t, 460> m_weapon_timestamps;
    std::bitset<460> m_valid_weapons;
    std::bitset<460> m_updated_weapons;
    
    // === Batch Reader ===
    BatchMemoryReader m_batch_reader;
    
    // === Timing ===
    std::chrono::steady_clock::time_point m_last_update;
    size_t m_frame_count = 0;
    
    // === Statistics ===
    CacheStats m_stats;
    CacheConfig m_config;
    
    // === Core Methods ===
    void scan_players_batch();
    void scan_bones_batch();
    void scan_weapons_batch();
    void update_cache_statistics();
    bool is_player_valid(const PlayerSnapshot& snapshot) const;
    bool should_update_player(int player_index) const;
    bool is_weapon_valid(const WeaponSnapshot& snapshot) const;
    bool should_update_weapon(int weapon_index) const;
    uintptr_t get_player_address(int player_index) const;
    uintptr_t get_bone_array_address(const PlayerSnapshot& snapshot) const;
    uintptr_t get_weapon_address(int weapon_index) const;
    
    // === Differential Updates ===
    struct PlayerDelta {
        bool core_changed = false;
        bool position_changed = false;
        bool combat_changed = false;
        bool bones_changed = false;
    };
    
    std::array<PlayerDelta, 64> m_player_deltas;
    PlayerDelta detect_player_changes(int player_index, const PlayerSnapshot& new_snapshot) const;
};

// === Global Instance ===
extern BatchOptimizedCache g_batch_cache;

// === Convenience Functions ===
inline const PlayerSnapshot* get_cached_player(int index) {
    return g_batch_cache.get_player_snapshot(index);
}

inline const PlayerBoneData* get_cached_bones(int index) {
    return g_batch_cache.get_player_bones(index);
}

inline const WeaponSnapshot* get_cached_weapon(int index) {
    return g_batch_cache.get_weapon_snapshot(index);
}

inline std::vector<int> get_cached_players() {
    return g_batch_cache.get_valid_player_indices();
}

inline std::vector<int> get_cached_weapons() {
    return g_batch_cache.get_valid_weapon_indices();
}
