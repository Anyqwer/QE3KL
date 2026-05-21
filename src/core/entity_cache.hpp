#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <chrono>
#include <unordered_map>
#include "../sdk/entity.hpp"

namespace core
{
    // Entity type bitmasks for fast filtering
    enum class EntityType : uint32_t {
        NONE = 0,
        PLAYER = 1 << 0,
        PROJECTILE = 1 << 1,
        INFERNO = 1 << 2,
        WEAPON = 1 << 3,
        PLANTED_C4 = 1 << 4,
        CARRIED_C4 = 1 << 5,
        
        // Combinations for batch filtering
        ALL_PLAYERS = PLAYER,
        ALL_WEAPONS = WEAPON | PLANTED_C4 | CARRIED_C4,
        ALL_EXPLOSIVES = PLANTED_C4 | INFERNO,
        VISIBLE_ENTITIES = PLAYER | WEAPON | PLANTED_C4,
        DANGEROUS_ENTITIES = PLANTED_C4 | INFERNO | PROJECTILE
    };

    // Bitmask operators for EntityType
    inline constexpr EntityType operator|(EntityType a, EntityType b) {
        return static_cast<EntityType>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }
    
    inline constexpr EntityType operator&(EntityType a, EntityType b) {
        return static_cast<EntityType>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    }
    
    inline constexpr bool has_type(EntityType mask, EntityType type) {
        return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(type)) != 0;
    }

    // Cached entity information
    struct CachedEntity
    {
        int32_t index = -1;
        uint32_t serial_number = 0;
        uintptr_t entity_ptr = 0;
        std::string class_name;
        uint64_t last_valid_time = 0;
        bool is_valid = false;
        
        // Classification - replaced with bitmask
        EntityType type = EntityType::NONE;
        uint32_t type_mask = static_cast<uint32_t>(EntityType::NONE);
        
        // Legacy compatibility (deprecated)
        bool is_player = false;
        bool is_projectile = false;
        bool is_inferno = false;
        bool is_weapon = false;
        bool is_planted_c4 = false;
        bool is_carried_c4 = false;
        
        // For quick lookup without string comparison
        uint32_t class_name_hash = 0;
    };
    
    class EntityCache
    {
    public:
        // Update cache - performs full scan only if needed
        void update();
        
        // Get cached entities by type
        std::vector<CachedEntity> get_players() const;
        /* GRENADES DISABLED - COMMENTED OUT TO FIX FPS LAGS
        std::vector<CachedEntity> get_projectiles() const;
        */
        std::vector<CachedEntity> get_infernos() const;
        std::vector<CachedEntity> get_weapons() const;
        std::vector<CachedEntity> get_all_valid() const;
        
        // Fast bitmask filtering methods
        std::vector<CachedEntity> get_by_mask(EntityType mask) const;
        std::vector<CachedEntity> get_players_fast() const;
        std::vector<CachedEntity> get_weapons_fast() const;
        std::vector<CachedEntity> get_explosives_fast() const;
        
        // Блочное сканирование с разными интервалами
        void update_block_based();
        void scan_entity_range(int32_t start, int32_t end, EntityType type);
        
        // Get specific entity by index (fast lookup)
        std::optional<CachedEntity> get_by_index(int32_t idx) const;
        
        // Force full rescan
        void invalidate() { m_last_full_scan = 0; }
        
        // Get cache statistics
        struct Stats
        {
            size_t total_cached = 0;
            size_t players = 0;
            size_t projectiles = 0;
            size_t infernos = 0;
            uint64_t last_update_ms = 0;
        };
        Stats get_stats() const;
        
    private:
        static constexpr uint64_t CACHE_TTL_MS = 200;  // Time-to-live for cached entries
        static constexpr uint64_t FULL_SCAN_INTERVAL_MS = 100;  // Full scan every 100ms
        
        // Блочное сканирование с разными интервалами
        static constexpr uint64_t PLAYER_SCAN_INTERVAL_MS = 16;    // 60Hz - каждый тик
  
        static constexpr uint64_t WEAPON_SCAN_INTERVAL_MS = 128;   // 7.5Hz - каждые 8 тиков
        
        std::unordered_map<int32_t, CachedEntity> m_cache;
        uint64_t m_last_full_scan = 0;
        
        // Таймеры для блочного сканирования
        uint64_t m_last_player_scan = 0;
        uint64_t m_last_weapon_scan = 0;
        
        // Classification helpers
        void classify_entity(CachedEntity& entity);
        bool is_valid_entity(const c_base_entity* entity) const;
    };
    
    // Global instance
    inline EntityCache g_entity_cache;
}
