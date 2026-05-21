#include "pch.hpp"
#include "batch_structures.hpp"
#include "../utils/memory.hpp"
#include <chrono>
#include <algorithm>

// Global batch offsets initialization
namespace batch_config {
    BatchOffsets g_batch_offsets = {};
}

BatchMemoryReader::BatchMemoryReader() {
    m_player_buffer.fill(0);
    m_bone_buffer.fill(0);
    
    // Initialize batch offsets (these would normally come from your offset system)
    // For now, using typical CS2 offsets - you should update these with your actual offsets
    batch_config::g_batch_offsets = {
        .player_core_offset = 0x0,        // Start of player data
        .player_position_offset = 0x40,   // Position data offset
        .player_combat_offset = 0x80,     // Combat data offset
        .bone_array_offset = 0x290,       // Bone array pointer
        .bone_head_offset = 0x10,         // Head bone index * sizeof(vector_t)
        .bone_neck_offset = 0x20,         // Neck bone index * sizeof(vector_t)
        .bone_chest_offset = 0x30,        // Chest bone index * sizeof(vector_t)
        .bone_left_hand_offset = 0x40,    // Left hand bone index * sizeof(vector_t)
        .bone_right_hand_offset = 0x50    // Right hand bone index * sizeof(vector_t)
    };
}

bool BatchMemoryReader::read_players_batch(const std::vector<uintptr_t>& player_addresses, 
                                          std::vector<PlayerSnapshot>& out_snapshots) {
    if (player_addresses.empty()) return false;
    
    const auto start_time = std::chrono::high_resolution_clock::now();
    
    out_snapshots.clear();
    out_snapshots.reserve(player_addresses.size());
    
    // Group players by memory proximity for better batching
    std::vector<std::pair<uintptr_t, size_t>> address_with_index;
    for (size_t i = 0; i < player_addresses.size(); ++i) {
        address_with_index.emplace_back(player_addresses[i], i);
    }
    
    // Sort by address for better memory access pattern
    std::sort(address_with_index.begin(), address_with_index.end());
    
    size_t successful_reads = 0;
    out_snapshots.resize(player_addresses.size());
    
    // Read each player's complete snapshot in a single RPM call
    for (size_t i = 0; i < address_with_index.size(); ++i) {
        const auto [player_addr, original_index] = address_with_index[i];
        
        if (!player_addr) continue;
        
        // Read entire player snapshot in one RPM call
        if (read_memory_batch(player_addr + batch_config::g_batch_offsets.player_core_offset, 
                             &out_snapshots[original_index], 
                             sizeof(PlayerSnapshot))) {
            successful_reads++;
            m_stats.bytes_per_batch += sizeof(PlayerSnapshot);
        }
    }
    
    // Update statistics
    const auto end_time = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    m_stats.total_rpm_calls += address_with_index.size();
    m_stats.players_per_batch = successful_reads;
    m_stats.avg_rpm_time_ms = duration.count() / 1000.0;
    
    return successful_reads > 0;
}

bool BatchMemoryReader::read_bones_batch(const std::vector<uintptr_t>& bone_array_addresses,
                                        std::vector<PlayerBoneData>& out_bones) {
    if (bone_array_addresses.empty()) return false;
    
    const auto start_time = std::chrono::high_resolution_clock::now();
    
    out_bones.clear();
    out_bones.reserve(bone_array_addresses.size());
    out_bones.resize(bone_array_addresses.size());
    
    size_t successful_reads = 0;
    
    // Read bone arrays for all players
    for (size_t i = 0; i < bone_array_addresses.size(); ++i) {
        const auto bone_array_addr = bone_array_addresses[i];
        if (!bone_array_addr) continue;
        
        // Calculate bone positions in the array
        const uintptr_t head_bone_addr = bone_array_addr + batch_config::g_batch_offsets.bone_head_offset;
        const uintptr_t neck_bone_addr = bone_array_addr + batch_config::g_batch_offsets.bone_neck_offset;
        const uintptr_t chest_bone_addr = bone_array_addr + batch_config::g_batch_offsets.bone_chest_offset;
        const uintptr_t left_hand_addr = bone_array_addr + batch_config::g_batch_offsets.bone_left_hand_offset;
        const uintptr_t right_hand_addr = bone_array_addr + batch_config::g_batch_offsets.bone_right_hand_offset;
        
        // Read all 5 bones in individual calls (still much better than reading all 64 bones)
        bool success = true;
        out_bones[i].head_bone = m_memory->read_t<vector_t>(head_bone_addr);
        success &= (out_bones[i].head_bone.m_x != 0 || out_bones[i].head_bone.m_y != 0 || out_bones[i].head_bone.m_z != 0);
        
        out_bones[i].neck_bone = m_memory->read_t<vector_t>(neck_bone_addr);
        success &= (out_bones[i].neck_bone.m_x != 0 || out_bones[i].neck_bone.m_y != 0 || out_bones[i].neck_bone.m_z != 0);
        
        out_bones[i].chest_bone = m_memory->read_t<vector_t>(chest_bone_addr);
        success &= (out_bones[i].chest_bone.m_x != 0 || out_bones[i].chest_bone.m_y != 0 || out_bones[i].chest_bone.m_z != 0);
        
        out_bones[i].left_hand_bone = m_memory->read_t<vector_t>(left_hand_addr);
        success &= (out_bones[i].left_hand_bone.m_x != 0 || out_bones[i].left_hand_bone.m_y != 0 || out_bones[i].left_hand_bone.m_z != 0);
        
        out_bones[i].right_hand_bone = m_memory->read_t<vector_t>(right_hand_addr);
        success &= (out_bones[i].right_hand_bone.m_x != 0 || out_bones[i].right_hand_bone.m_y != 0 || out_bones[i].right_hand_bone.m_z != 0);
        
        if (success) {
            successful_reads++;
            m_stats.bytes_per_batch += sizeof(PlayerBoneData);
            m_stats.total_rpm_calls += 5; // 5 RPM calls per player for bones
        }
    }
    
    // Update statistics
    const auto end_time = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    m_stats.avg_rpm_time_ms = duration.count() / 1000.0;
    
    return successful_reads > 0;
}

bool BatchMemoryReader::read_weapons_batch(const std::vector<uintptr_t>& weapon_addresses,
                                          std::vector<WeaponSnapshot>& out_weapons) {
    if (weapon_addresses.empty()) return false;
    
    const auto start_time = std::chrono::high_resolution_clock::now();
    
    out_weapons.clear();
    out_weapons.reserve(weapon_addresses.size());
    
    // Group weapons by memory proximity for better batching
    std::vector<std::pair<uintptr_t, size_t>> address_with_index;
    for (size_t i = 0; i < weapon_addresses.size(); ++i) {
        address_with_index.emplace_back(weapon_addresses[i], i);
    }
    
    // Sort by address for better memory access pattern
    std::sort(address_with_index.begin(), address_with_index.end());
    
    size_t successful_reads = 0;
    out_weapons.resize(weapon_addresses.size());
    
    // Read each weapon's complete snapshot in a single RPM call
    for (size_t i = 0; i < address_with_index.size(); ++i) {
        const auto [weapon_addr, original_index] = address_with_index[i];
        
        if (!weapon_addr) continue;
        
        // Read entire weapon snapshot in one RPM call
        if (read_memory_batch(weapon_addr + batch_config::g_batch_offsets.weapon_core_offset,
                             &out_weapons[original_index],
                             sizeof(WeaponSnapshot))) {
            successful_reads++;
            m_stats.bytes_per_batch += sizeof(WeaponSnapshot);
        }
    }
    
    // Update statistics
    const auto end_time = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    m_stats.avg_rpm_time_ms = duration.count() / 1000.0;
    
    m_stats.total_rpm_calls += weapon_addresses.size();
    m_stats.players_per_batch = successful_reads;
    
    return successful_reads > 0;
}

bool BatchMemoryReader::read_memory_batch(uintptr_t base_address, void* buffer, size_t size) {
    if (!base_address || !buffer || size == 0) return false;
    
    try {
        // Use the existing memory interface for batch reading
        // This would ideally use ReadProcessMemory with a large buffer
        return m_memory->read_raw(base_address, static_cast<uint8_t*>(buffer), size);
    }
    catch (...) {
        return false;
    }
}
