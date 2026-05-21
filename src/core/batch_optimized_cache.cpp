#include "pch.hpp"
#include "batch_optimized_cache.hpp"
#include "../core/interfaces.hpp"
#include "../utils/memory.hpp"

// Global batch cache instance
BatchOptimizedCache g_batch_cache;

BatchOptimizedCache::BatchOptimizedCache() {
    // Initialize all data structures
    m_player_snapshots.fill({});
    m_player_bones.fill({});
    m_player_timestamps.fill(0);
    m_valid_players.reset();
    m_updated_players.reset();
    m_player_deltas.fill({});
    
    // Initialize weapon cache
    m_weapon_snapshots.fill({});
    m_weapon_timestamps.fill(0);
    m_valid_weapons.reset();
    m_updated_weapons.reset();
    
    m_last_update = std::chrono::steady_clock::now();
    m_frame_count = 0;
}

void BatchOptimizedCache::update() {
    const auto start_time = std::chrono::high_resolution_clock::now();
    
    // Reset frame-specific data
    m_updated_players.reset();
    m_updated_weapons.reset();
    m_stats.rpm_calls_per_frame = 0;
    m_stats.players_cached = 0;
    m_stats.bones_cached = 0;
    m_stats.weapons_cached = 0;
    
    // Update timing
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_update).count();
    
    // Scan players using batch reading
    scan_players_batch();
    
    // Scan weapons at reduced frequency
    if (m_frame_count % m_config.weapon_update_interval.count() == 0) {
        scan_weapons_batch();
    }
    
    // Update bones at reduced frequency
    if (m_config.enable_bone_caching && (m_frame_count % m_config.bone_update_interval.count() == 0)) {
        scan_bones_batch();
    }
    
    // Update statistics
    const auto end_time = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    m_stats.avg_batch_time_ms = duration.count() / 1000.0;
    m_last_update = now;
    m_frame_count++;
    
    update_cache_statistics();
}

void BatchOptimizedCache::scan_players_batch() {
    if (!i::m_game_entity_system) return;
    
    // Clear old player data
    m_valid_players.reset();
    
    // Scan entity list for players (0-63 range)
    for (int i = 0; i < m_config.max_players; ++i) {
        const auto entity = i::m_game_entity_system->get(i);
        if (!entity) continue;
        
        const auto handle = entity->get_ref_e_handle();
        if (!handle.is_valid()) continue;
        
        // Validate as player controller
        const auto controller = reinterpret_cast<c_cs_player_controller*>(entity);
        if (!controller) continue;
        
        const auto pawn = controller->get_player_pawn();
        if (!pawn) continue;
        
        // Quick validation
        const auto health = pawn->m_iHealth();
        const auto team = controller->m_iTeamNum();
        
        if (health > 0 && health <= 100 && (team == e_team::t || team == e_team::ct)) {
            // Create player snapshot from pawn data
            PlayerSnapshot snapshot = {};
            
            // Core data
            snapshot.core.m_iHealth = health;
            snapshot.core.m_iTeamNum = static_cast<int32_t>(team);
            snapshot.core.m_fFlags = 0; // Not available in entity classes, set to default
            snapshot.core.m_lifeState = 0; // LIFE_ALIVE - not available, set to default
            snapshot.core.m_iIDEntIndex = i; // Use entity index as ID since not available in entity
            
            // Position data
            snapshot.position.m_vecOrigin = pawn->get_scene_origin();
            snapshot.position.m_vecVelocity = pawn->m_vecAbsVelocity();
            snapshot.position.m_angEyeAngles = pawn->m_angEyeAngles();
            
            // Combat data
            if (pawn->m_pWeaponServices()) {
                snapshot.combat.m_pActiveWeapon = pawn->m_pWeaponServices()->m_hActiveWeapon().get_entry_idx();
            }
            
            // Scene node for bones
            snapshot.m_pGameSceneNode = reinterpret_cast<uintptr_t>(pawn->m_pGameSceneNode());
            
            // Update cache
            m_player_snapshots[i] = snapshot;
            m_player_timestamps[i] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            m_valid_players.set(i);
            m_updated_players.set(i);
            
            m_stats.players_cached++;
            m_stats.rpm_calls_per_frame += 8; // Approximate RPM calls for this player
        }
    }
}

void BatchOptimizedCache::scan_weapons_batch() {
    if (!i::m_game_entity_system) return;
    
    // Clear old weapon data
    m_valid_weapons.reset();
    
    // Scan entity list for weapons (65-524 range)
    for (int i = 65; i <= 524; ++i) {
        const auto entity = i::m_game_entity_system->get(i);
        if (!entity) continue;
        
        const auto handle = entity->get_ref_e_handle();
        if (!handle.is_valid()) continue;
        
        // Check if this is a weapon entity
        const auto weapon = reinterpret_cast<c_base_player_weapon*>(entity);
        if (!weapon) continue;
        
        // Quick validation - check if weapon has valid position
        const auto origin = weapon->get_scene_origin();
        if (origin.is_zero()) continue;
        
        // Create weapon snapshot
        WeaponSnapshot snapshot = {};
        
        // Core weapon data
        snapshot.m_vecOrigin = origin;
        // Get owner entity handle - m_hOwnerEntity returns c_base_entity*, need to get its handle
        const auto owner_entity = weapon->m_hOwnerEntity();
        if (owner_entity) {
            snapshot.m_hOwnerEntity = owner_entity->get_ref_e_handle().get_entry_idx();
        } else {
            snapshot.m_hOwnerEntity = 0;
        }
        
        // Get weapon type from weapon data
        const auto weapon_v_data = weapon->m_WeaponData();
        if (weapon_v_data) {
            snapshot.m_iWeaponType = static_cast<int32_t>(weapon_v_data->m_WeaponType());
        }
        
        // Update cache
        const int weapon_cache_index = i - 65; // Convert to 0-based index for cache arrays
        if (weapon_cache_index >= 0 && weapon_cache_index < m_config.max_weapons) {
            m_weapon_snapshots[weapon_cache_index] = snapshot;
            m_weapon_timestamps[weapon_cache_index] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            m_valid_weapons.set(weapon_cache_index);
            m_updated_weapons.set(weapon_cache_index);
            
            m_stats.weapons_cached++;
            m_stats.rpm_calls_per_frame += 4; // Approximate RPM calls for this weapon
        }
    }
}

void BatchOptimizedCache::scan_bones_batch() {
    // Collect bone array addresses for valid players
    std::vector<uintptr_t> bone_addresses;
    std::vector<int> player_indices;
    
    for (int i = 0; i < m_config.max_players; ++i) {
        if (!m_valid_players.test(i)) continue;
        
        const uintptr_t bone_array_addr = get_bone_array_address(m_player_snapshots[i]);
        if (bone_array_addr) {
            bone_addresses.push_back(bone_array_addr);
            player_indices.push_back(i);
        }
    }
    
    if (bone_addresses.empty()) return;
    
    // Batch read bone data
    std::vector<PlayerBoneData> new_bones;
    const bool success = m_batch_reader.read_bones_batch(bone_addresses, new_bones);
    
    if (!success) return;
    
    // Update bone cache
    for (size_t i = 0; i < player_indices.size(); ++i) {
        const int player_index = player_indices[i];
        m_player_bones[player_index] = new_bones[i];
        m_stats.bones_cached++;
    }
    
    // Update RPM call statistics
    const auto& batch_stats = m_batch_reader.get_stats();
    m_stats.rpm_calls_per_frame += batch_stats.total_rpm_calls;
}

const PlayerSnapshot* BatchOptimizedCache::get_player_snapshot(int player_index) const {
    if (player_index < 0 || player_index >= m_config.max_players) return nullptr;
    if (!m_valid_players.test(player_index)) return nullptr;
    
    // Check if cache entry is still valid
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    if (now - m_player_timestamps[player_index] > m_config.cache_ttl.count()) {
        return nullptr; // Cache entry expired
    }
    
    return &m_player_snapshots[player_index];
}

const PlayerBoneData* BatchOptimizedCache::get_player_bones(int player_index) const {
    if (player_index < 0 || player_index >= m_config.max_players) return nullptr;
    if (!m_valid_players.test(player_index)) return nullptr;
    
    return &m_player_bones[player_index];
}

std::vector<int> BatchOptimizedCache::get_valid_player_indices() const {
    std::vector<int> indices;
    indices.reserve(m_valid_players.count());
    
    for (int i = 0; i < m_config.max_players; ++i) {
        if (m_valid_players.test(i)) {
            indices.push_back(i);
        }
    }
    
    return indices;
}

bool BatchOptimizedCache::is_player_valid(const PlayerSnapshot& snapshot) const {
    return snapshot.core.m_iHealth > 0 && 
           snapshot.core.m_iTeamNum > 0 && 
           snapshot.core.m_iTeamNum < 4 && // Valid team numbers (1-3)
           snapshot.core.m_lifeState == 0; // LIFE_ALIVE
}

bool BatchOptimizedCache::should_update_player(int player_index) const {
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // Update if not cached or cache expired
    if (!m_valid_players.test(player_index)) return true;
    if (now - m_player_timestamps[player_index] > m_config.cache_ttl.count()) return true;
    
    // For differential updates, check if any relevant data changed
    if (m_config.enable_differential_updates) {
        // This would require additional logic to detect changes without reading
        // For now, we'll update based on timing
        return false;
    }
    
    return false;
}

uintptr_t BatchOptimizedCache::get_player_address(int player_index) const {
    if (!i::m_game_entity_system) return 0;
    
    const auto entity = i::m_game_entity_system->get(player_index);
    if (!entity) return 0;
    
    // Check if this is a player controller
    const auto controller = reinterpret_cast<c_cs_player_controller*>(entity);
    if (!controller) return 0;
    
    const auto pawn = controller->get_player_pawn();
    return pawn ? reinterpret_cast<uintptr_t>(pawn) : 0;
}

uintptr_t BatchOptimizedCache::get_bone_array_address(const PlayerSnapshot& snapshot) const {
    if (!snapshot.m_pGameSceneNode) return 0;
    
    // Read bone array pointer from game scene node
    try {
        const uintptr_t scene_node_addr = snapshot.m_pGameSceneNode;
        return m_memory->read_t<uintptr_t>(scene_node_addr + batch_config::g_batch_offsets.bone_array_offset);
    }
    catch (...) {
        return 0;
    }
}

BatchOptimizedCache::PlayerDelta BatchOptimizedCache::detect_player_changes(
    int player_index, const PlayerSnapshot& new_snapshot) const {
    
    PlayerDelta delta;
    
    if (!m_valid_players.test(player_index)) {
        // New player - all data changed
        delta.core_changed = true;
        delta.position_changed = true;
        delta.combat_changed = true;
        delta.bones_changed = true;
        return delta;
    }
    
    const auto& old_snapshot = m_player_snapshots[player_index];
    
    // Detect changes in each category
    delta.core_changed = (old_snapshot.core.m_iHealth != new_snapshot.core.m_iHealth ||
                         old_snapshot.core.m_iTeamNum != new_snapshot.core.m_iTeamNum ||
                         old_snapshot.core.m_fFlags != new_snapshot.core.m_fFlags);
    
    // Vector comparison using component-wise comparison
    delta.position_changed = (
        old_snapshot.position.m_vecOrigin.m_x != new_snapshot.position.m_vecOrigin.m_x ||
        old_snapshot.position.m_vecOrigin.m_y != new_snapshot.position.m_vecOrigin.m_y ||
        old_snapshot.position.m_vecOrigin.m_z != new_snapshot.position.m_vecOrigin.m_z ||
        old_snapshot.position.m_vecVelocity.m_x != new_snapshot.position.m_vecVelocity.m_x ||
        old_snapshot.position.m_vecVelocity.m_y != new_snapshot.position.m_vecVelocity.m_y ||
        old_snapshot.position.m_vecVelocity.m_z != new_snapshot.position.m_vecVelocity.m_z ||
        old_snapshot.position.m_angEyeAngles.m_x != new_snapshot.position.m_angEyeAngles.m_x ||
        old_snapshot.position.m_angEyeAngles.m_y != new_snapshot.position.m_angEyeAngles.m_y ||
        old_snapshot.position.m_angEyeAngles.m_z != new_snapshot.position.m_angEyeAngles.m_z
    );
    
    delta.combat_changed = (old_snapshot.combat.m_iShotsFired != new_snapshot.combat.m_iShotsFired ||
                           old_snapshot.combat.m_pActiveWeapon != new_snapshot.combat.m_pActiveWeapon ||
                           old_snapshot.combat.m_flFlashDuration != new_snapshot.combat.m_flFlashDuration);
    
    return delta;
}

void BatchOptimizedCache::update_cache_statistics() {
    // Calculate cache hit rate
    const size_t total_requests = m_stats.cache_hits + m_stats.cache_misses;
    if (total_requests > 0) {
        m_stats.cache_hit_rate = static_cast<double>(m_stats.cache_hits) / total_requests * 100.0;
    }
    
    // Update batch reader statistics
    m_stats.rpm_calls_per_frame += m_batch_reader.get_stats().total_rpm_calls;
}

// === Weapon Methods ===
const WeaponSnapshot* BatchOptimizedCache::get_weapon_snapshot(int weapon_index) const {
    if (weapon_index < 65 || weapon_index > 524) return nullptr;
    
    const int cache_index = weapon_index - 65;
    if (cache_index >= 0 && cache_index < m_config.max_weapons && m_valid_weapons.test(cache_index)) {
        return &m_weapon_snapshots[cache_index];
    }
    
    return nullptr;
}

std::vector<int> BatchOptimizedCache::get_valid_weapon_indices() const {
    std::vector<int> valid_indices;
    valid_indices.reserve(m_valid_weapons.count());
    
    for (int i = 0; i < m_config.max_weapons; ++i) {
        if (m_valid_weapons.test(i)) {
            valid_indices.push_back(i + 65); // Convert back to entity index
        }
    }
    
    return valid_indices;
}

bool BatchOptimizedCache::is_weapon_valid(const WeaponSnapshot& snapshot) const {
    return !snapshot.m_vecOrigin.is_zero() && 
           snapshot.m_iWeaponType >= 0 && 
           snapshot.m_iWeaponType <= 14; // Valid weapon types
}

bool BatchOptimizedCache::should_update_weapon(int weapon_index) const {
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    const int cache_index = weapon_index - 65;
    if (cache_index < 0 || cache_index >= m_config.max_weapons) return false;
    
    // Update if not cached or cache expired
    if (!m_valid_weapons.test(cache_index)) return true;
    if (now - m_weapon_timestamps[cache_index] > m_config.cache_ttl.count()) return true;
    
    return false;
}

uintptr_t BatchOptimizedCache::get_weapon_address(int weapon_index) const {
    if (!i::m_game_entity_system) return 0;
    
    const auto entity = i::m_game_entity_system->get(weapon_index);
    if (!entity) return 0;
    
    const auto handle = entity->get_ref_e_handle();
    if (!handle.is_valid()) return 0;
    
    // Check if this is a weapon entity
    const auto weapon = reinterpret_cast<c_base_player_weapon*>(entity);
    return weapon ? reinterpret_cast<uintptr_t>(weapon) : 0;
}
