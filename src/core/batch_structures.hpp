#pragma once
#include "../sdk/datatypes/vector.hpp"
#include <cstdint>

// Use the correct vector type from the SDK
using vector_t = f_vector;

// === Batch Reading Structures for RPM Optimization ===
// These structures are designed to read multiple fields in a single RPM call

#pragma pack(push, 1) // Ensure no padding between fields

// Core player data that changes frequently (health, team, flags)
struct PlayerCoreData {
    int32_t m_iHealth = 0;
    int32_t m_iTeamNum = 0;
    uint32_t m_fFlags = 0;
    uint32_t m_lifeState = 0;
    int32_t m_iIDEntIndex = 0;
};

// Position and movement data (changes every frame)
struct PlayerPositionData {
    vector_t m_vecOrigin = {};      // World position
    vector_t m_vecVelocity = {};    // Movement velocity  
    vector_t m_angEyeAngles = {};   // View angles
    float m_flSimulationTime = 0.0f;
    float m_flOldSimulationTime = 0.0f;
};

// Combat-related data (weapons, shooting)
struct PlayerCombatData {
    int32_t m_iShotsFired = 0;
    uint32_t m_pActiveWeapon = 0;   // Weapon entity handle
    uint32_t m_pAimPunchServices = 0;
    float m_flFlashDuration = 0.0f;
    uint32_t m_bIsScoped = 0;
};

// Complete player snapshot (all essential data)
struct PlayerSnapshot {
    PlayerCoreData core;
    PlayerPositionData position;
    PlayerCombatData combat;
    uint32_t m_pGameSceneNode = 0;  // For bone access
    uint32_t m_pCollision = 0;      // For collision detection
};

// Bone data structure for batch reading (5 essential bones)
struct PlayerBoneData {
    vector_t head_bone;
    vector_t neck_bone;
    vector_t chest_bone;
    vector_t left_hand_bone;
    vector_t right_hand_bone;
};

// Weapon data structure for batch reading
struct WeaponSnapshot {
    int32_t m_iWeaponType = 0;
    vector_t m_vecOrigin = {};
    int32_t m_iClip1 = 0;
    int32_t m_iClip2 = 0;
    uint32_t m_hOwnerEntity = 0;
};

#pragma pack(pop)

// === Batch Reading Configuration ===
namespace batch_config {
    constexpr size_t MAX_PLAYERS = 64;        // 0-64: Players
    constexpr size_t MAX_WEAPONS = 460;         // 65-524: Weapons (524-65+1=460)
    constexpr size_t BATCH_BUFFER_SIZE = sizeof(PlayerSnapshot) * MAX_PLAYERS;
    constexpr size_t BONE_BATCH_SIZE = sizeof(PlayerBoneData) * MAX_PLAYERS;
    constexpr size_t WEAPON_BATCH_SIZE = sizeof(WeaponSnapshot) * MAX_WEAPONS;
    
    // Offsets for batch reading (will be populated at runtime)
    struct BatchOffsets {
        size_t player_core_offset = 0;
        size_t player_position_offset = 0;
        size_t player_combat_offset = 0;
        size_t bone_array_offset = 0;
        size_t bone_head_offset = 0;
        size_t bone_neck_offset = 0;
        size_t bone_chest_offset = 0;
        size_t bone_left_hand_offset = 0;
        size_t bone_right_hand_offset = 0;
        size_t weapon_core_offset = 0;
        size_t weapon_position_offset = 0;
    };
    
    extern BatchOffsets g_batch_offsets;
}

// === Batch Memory Reader Interface ===
class BatchMemoryReader {
public:
    BatchMemoryReader();
    ~BatchMemoryReader() = default;
    
    // Read multiple players in a single operation
    bool read_players_batch(const std::vector<uintptr_t>& player_addresses, 
                           std::vector<PlayerSnapshot>& out_snapshots);
    
    // Read bone data for multiple players
    bool read_bones_batch(const std::vector<uintptr_t>& bone_array_addresses,
                         std::vector<PlayerBoneData>& out_bones);
    
    // Read weapon data for multiple weapons
    bool read_weapons_batch(const std::vector<uintptr_t>& weapon_addresses,
                           std::vector<WeaponSnapshot>& out_weapons);
    
    // Get performance statistics
    struct Stats {
        size_t total_rpm_calls = 0;
        size_t players_per_batch = 0;
        size_t bytes_per_batch = 0;
        double avg_rpm_time_ms = 0.0;
    };
    
    const Stats& get_stats() const { return m_stats; }
    void reset_stats() { m_stats = {}; }
    
private:
    std::array<uint8_t, batch_config::BATCH_BUFFER_SIZE> m_player_buffer;
    std::array<uint8_t, batch_config::BONE_BATCH_SIZE> m_bone_buffer;
    std::array<uint8_t, batch_config::WEAPON_BATCH_SIZE> m_weapon_buffer;
    Stats m_stats;
    
    bool read_memory_batch(uintptr_t base_address, void* buffer, size_t size);
};
