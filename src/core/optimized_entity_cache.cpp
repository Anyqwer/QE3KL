#include "pch.hpp"
#include "optimized_entity_cache.hpp"
#include "../common.hpp"
#include "../ext/fnv1a/fnv1a.hpp"

namespace core
{
    // === Static Hash Values for Fast Classification ===
    // Pre-computed FNV1a hashes for common CS2 entity classes
    constexpr uint32_t OptimizedEntityCache::HASH_PLAYER_CONTROLLER;
    constexpr uint32_t OptimizedEntityCache::HASH_PLANTED_C4;
    constexpr uint32_t OptimizedEntityCache::HASH_C4;
    constexpr uint32_t OptimizedEntityCache::HASH_INFERNO;
    
    // === Performance Monitoring ===
    // Track RPM calls and performance improvements
    struct PerformanceMetrics {
        uint64_t total_rpm_calls = 0;
        uint64_t player_scan_calls = 0;
        uint64_t weapon_scan_calls = 0;
        uint64_t last_metrics_reset = 0;
        
        void reset() {
            auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            
            if (now - last_metrics_reset > 1000000) { // Reset every second
                total_rpm_calls = 0;
                player_scan_calls = 0;
                weapon_scan_calls = 0;
                last_metrics_reset = now;
            }
        }
        
        void log_stats() {
            reset();
            
            static uint64_t last_log = 0;
            auto now = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            
            if (now - last_log > 5000000) { // Log every 5 seconds
                printf("[OptimizedEntityCache] Performance Stats:\n");
                printf("  Player RPM calls: %llu/sec\n", player_scan_calls);
                printf("  Weapon RPM calls: %llu/sec\n", weapon_scan_calls);
                printf("  Total RPM calls: %llu/sec\n", total_rpm_calls);
                printf("  Reduction vs full scan: %.1f%%\n", 
                       ((1024.0 - total_rpm_calls) / 1024.0) * 100.0);
                last_log = now;
            }
        }
    };
    
    static PerformanceMetrics g_perf_metrics;
    
    // === Optimized Entity Classification ===
    // Enhanced classification with caching and fast paths
    EntityType OptimizedEntityCache::classify_entity_fast(const c_base_entity* entity) const
    {
        if (!entity) return EntityType::NONE;
        
        // Fast path: try to get cached entity first
        const auto handle = entity->get_ref_e_handle();
        if (!handle.is_valid()) return EntityType::NONE;
        
        const int32_t entity_index = handle.get_entry_index();
        if (entity_index >= 0 && entity_index < MAX_ENTITIES) {
            auto it = m_entity_cache.find(entity_index);
            if (it != m_entity_cache.end() && it->second.is_valid) {
                return it->second.type; // Return cached type
            }
        }
        
        // Classification using optimized string checks
        const auto class_name = entity->get_schema_class_name();
        if (class_name.empty()) return EntityType::NONE;
        
        // === Fast String Classification ===
        // Order by frequency: Players > Weapons > Explosives > Projectiles
        
        // 1. Player Controllers (most common)
        if (class_name[0] == 'C' && class_name.find("PlayerController") != std::string::npos) {
            return EntityType::PLAYER;
        }
        
        // 2. Weapons (second most common)
        if (class_name.find("Weapon") != std::string::npos) {
            // Fast weapon type detection
            if (class_name.find("C4") != std::string::npos) {
                return EntityType::CARRIED_C4;
            }
            return EntityType::WEAPON;
        }
        
        // 3. Explosives (less common but critical)
        if (class_name.find("PlantedC4") != std::string::npos) {
            return EntityType::PLANTED_C4;
        }
        
        if (class_name.find("Inferno") != std::string::npos) {
            return EntityType::INFERNO;
        }
        
        // 4. Projectiles (least common, disabled for performance)
        /*
        if (class_name.find("Projectile") != std::string::npos ||
            class_name.find("Grenade") != std::string::npos) {
            return EntityType::PROJECTILE;
        }
        */
        
        return EntityType::NONE;
    }
    
    // === Enhanced Range Scanning with Validation ===
    void OptimizedEntityCache::scan_entity_range_optimized(int32_t start, int32_t end, EntityType type)
    {
        const uint64_t current_time = get_time_us();
        
        // Performance tracking
        g_perf_metrics.total_rpm_calls += (end - start);
        
        switch (type) {
            case EntityType::PLAYER:
                g_perf_metrics.player_scan_calls += (end - start);
                break;
            case EntityType::WEAPON:
                g_perf_metrics.weapon_scan_calls += (end - start);
                break;
        }
        
        // Select appropriate bitmap
        std::array<uint64_t, BITMAP_BLOCKS>* target_bitmap = nullptr;
        switch (type) {
            case EntityType::PLAYER:    target_bitmap = &m_player_bitmap; break;
            case EntityType::PROJECTILE: target_bitmap = &m_projectile_bitmap; break;
            case EntityType::WEAPON:    target_bitmap = &m_weapon_bitmap; break;
            default: return;
        }
        
        // === Optimized Entity Scanning ===
        // Batch processing with minimal overhead
        for (int32_t idx = start; idx < end; idx++) {
            const auto entity = i::m_game_entity_system->get(idx);
            const bool is_valid = entity && entity->get_ref_e_handle().is_valid();
            
            if (is_valid) {
                // === Valid Entity Path ===
                set_bitmap_bit(*target_bitmap, idx);
                m_entity_generation[idx] = m_current_generation;
                
                // Check if we need to cache full entity data
                auto it = m_entity_cache.find(idx);
                if (it == m_entity_cache.end()) {
                    // === New Entity - Full Cache ===
                    OptimizedCachedEntity cached_entity;
                    cached_entity.index = idx;
                    cached_entity.entity_ptr = reinterpret_cast<uintptr_t>(entity);
                    cached_entity.serial_number = entity->get_ref_e_handle().get_serial_number();
                    cached_entity.last_valid_time = current_time;
                    cached_entity.is_valid = true;
                    
                    // Fast classification
                    cached_entity.type = classify_entity_fast(entity);
                    cached_entity.type_mask = static_cast<uint32_t>(cached_entity.type);
                    
                    // Update all type flags
                    cached_entity.is_player = (cached_entity.type == EntityType::PLAYER);
                    cached_entity.is_projectile = (cached_entity.type == EntityType::PROJECTILE);
                    cached_entity.is_weapon = (cached_entity.type == EntityType::WEAPON);
                    cached_entity.is_planted_c4 = (cached_entity.type == EntityType::PLANTED_C4);
                    cached_entity.is_carried_c4 = (cached_entity.type == EntityType::CARRIED_C4);
                    cached_entity.is_inferno = (cached_entity.type == EntityType::INFERNO);
                    
                    // Cache class name hash
                    const auto class_name = entity->get_schema_class_name();
                    cached_entity.class_name_hash = fnv1a::hash(class_name);
                    
                    m_entity_cache[idx] = std::move(cached_entity);
                } else {
                    // === Existing Entity - Update Only ===
                    auto& cached = it->second;
                    cached.last_valid_time = current_time;
                    cached.is_valid = true;
                    cached.entity_ptr = reinterpret_cast<uintptr_t>(entity);
                    cached.serial_number = entity->get_ref_e_handle().get_serial_number();
                    
                    // Re-classify if needed (rare)
                    if (cached.type == EntityType::NONE) {
                        cached.type = classify_entity_fast(entity);
                        cached.type_mask = static_cast<uint32_t>(cached_entity.type);
                        
                        // Update flags
                        cached.is_player = (cached.type == EntityType::PLAYER);
                        cached.is_projectile = (cached.type == EntityType::PROJECTILE);
                        cached.is_weapon = (cached.type == EntityType::WEAPON);
                        cached.is_planted_c4 = (cached.type == EntityType::PLANTED_C4);
                        cached.is_carried_c4 = (cached.type == EntityType::CARRIED_C4);
                        cached.is_inferno = (cached.type == EntityType::INFERNO);
                    }
                }
                
                // Update explosive bitmap if needed
                if (cached_entity.type == EntityType::PLANTED_C4 || 
                    cached_entity.type == EntityType::INFERNO) {
                    set_bitmap_bit(m_explosive_bitmap, idx);
                }
                
            } else {
                // === Invalid Entity Path ===
                clear_bitmap_bit(*target_bitmap, idx);
                clear_bitmap_bit(m_explosive_bitmap, idx);
                
                // Invalidate cache entry
                auto it = m_entity_cache.find(idx);
                if (it != m_entity_cache.end()) {
                    it->second.is_valid = false;
                }
            }
        }
        
        // Log performance metrics periodically
        g_perf_metrics.log_stats();
    }
    
    // === Enhanced Cleanup with Memory Management ===
    void OptimizedEntityCache::cleanup_stale_entities()
    {
        const uint64_t current_time = get_time_us();
        const uint64_t cutoff_time = current_time - ENTITY_TTL_US;
        
        size_t cleaned_count = 0;
        auto it = m_entity_cache.begin();
        
        while (it != m_entity_cache.end()) {
            if (it->second.last_valid_time < cutoff_time) {
                // Clear all bitmap bits for this entity
                clear_bitmap_bit(m_player_bitmap, it->first);
                clear_bitmap_bit(m_projectile_bitmap, it->first);
                clear_bitmap_bit(m_weapon_bitmap, it->first);
                clear_bitmap_bit(m_explosive_bitmap, it->first);
                
                it = m_entity_cache.erase(it);
                cleaned_count++;
            } else {
                ++it;
            }
        }
        
        if (cleaned_count > 0) {
            printf("[OptimizedEntityCache] Cleaned up %zu stale entities\n", cleaned_count);
        }
    }
    
    // === Public Interface Implementation ===
    
    // Ultra-fast player retrieval for main loop
    std::vector<int32_t> OptimizedEntityCache::get_player_indices_fast() const
    {
        std::vector<int32_t> players;
        players.reserve(32); // Reserve for typical player count
        
        // Only check player range (0-63) using bitmap
        for (int32_t idx = 0; idx < PLAYERS_RANGE; idx++) {
            if (is_bitmap_bit_set(m_player_bitmap, idx) && 
                m_entity_generation[idx] == m_current_generation) {
                
                // Double-check entity is still valid
                auto it = m_entity_cache.find(idx);
                if (it != m_entity_cache.end() && it->second.is_valid && it->second.is_player) {
                    players.push_back(idx);
                }
            }
        }
        
        return players;
    }
    
    // Get entities by type with filtering
    std::vector<OptimizedCachedEntity> OptimizedEntityCache::get_entities_by_type(EntityType type) const
    {
        std::vector<OptimizedCachedEntity> entities;
        entities.reserve(64);
        
        const uint32_t type_mask = static_cast<uint32_t>(type);
        
        for (const auto& [idx, entity] : m_entity_cache) {
            if (entity.is_valid && (entity.type_mask & type_mask)) {
                entities.push_back(entity);
            }
        }
        
        return entities;
    }
    
    // Get specific entity with validation
    std::optional<OptimizedCachedEntity> OptimizedEntityCache::get_entity_by_index(int32_t index) const
    {
        if (index < 0 || index >= MAX_ENTITIES) {
            return std::nullopt;
        }
        
        auto it = m_entity_cache.find(index);
        if (it != m_entity_cache.end() && it->second.is_valid) {
            return it->second;
        }
        
        return std::nullopt;
    }
    
    // Performance statistics with detailed metrics
    OptimizedEntityCache::PerformanceStats OptimizedEntityCache::get_performance_stats() const
    {
        PerformanceStats stats;
        stats.total_cached = m_entity_cache.size();
        stats.current_generation = m_current_generation;
        stats.last_update_us = get_time_us();
        
        // Count entities by type
        for (const auto& [idx, entity] : m_entity_cache) {
            if (!entity.is_valid) continue;
            
            if (entity.is_player) stats.player_count++;
            if (entity.is_projectile) stats.projectile_count++;
            if (entity.is_weapon) stats.weapon_count++;
            if (entity.is_planted_c4 || entity.is_inferno) stats.explosive_count++;
        }
        
        // Calculate performance improvements
        const size_t old_rpm_per_second = 1024 * 60; // Full scan at 60Hz
        const size_t new_rpm_per_second = 
            (PLAYERS_RANGE * 60) +                    // Players: 64 * 60
            (PROJECTILES_RANGE * 15) +                // Projectiles: 448 * 15  
            (WEAPONS_RANGE * 8);                      // Weapons: 512 * 8
        
        stats.rpm_reduction = ((double)(old_rpm_per_second - new_rpm_per_second) / old_rpm_per_second) * 100.0;
        stats.rpm_calls_saved = old_rpm_per_second - new_rpm_per_second;
        
        return stats;
    }
    
    // Force complete cache invalidation
    void OptimizedEntityCache::invalidate()
    {
        m_current_generation++;
        
        // Clear all bitmaps
        m_player_bitmap.fill(0);
        m_projectile_bitmap.fill(0);
        m_weapon_bitmap.fill(0);
        m_explosive_bitmap.fill(0);
        
        // Reset generation tracking
        m_entity_generation.fill(0);
        
        // Clear entity cache
        m_entity_cache.clear();
        
        // Reset timing
        m_last_player_scan = 0;
        m_last_weapon_scan = 0;
        m_last_cleanup = 0;
        
        printf("[OptimizedEntityCache] Cache invalidated\n");
    }
    
    // Check if cache is properly initialized
    bool OptimizedEntityCache::is_ready() const
    {
        return m_current_generation > 1 && !m_entity_cache.empty();
    }
}
