#include "pch.hpp"
#include "entity_cache.hpp"
#include "interfaces.hpp"
#include "../common.hpp"
#include "../ext/fnv1a/fnv1a.hpp"
#include "../sdk/entity.hpp"

namespace core
{
    void EntityCache::update()
{
    const auto now = GetTickCount64();
    
    // Блочное сканирование вместо полного скана каждый раз
    update_block_based();
    
    // Полный скан только для очистки кэша (редко)
    if (now - m_last_full_scan < FULL_SCAN_INTERVAL_MS * 10) // Каждую секунду
        return;
    
    m_last_full_scan = now;
        
        // Remove expired entries first
        std::erase_if(m_cache, [now](const auto& pair) {
            return (now - pair.second.last_valid_time) > CACHE_TTL_MS;
        });
        
        // Scan limited range - projectiles rarely exceed index 1024
        // Players are 0-64, projectiles 64-512, weapons dropped 512-1024
        constexpr int32_t SCAN_LIMIT = 1024;
        
        for (int32_t idx = 0; idx < SCAN_LIMIT; idx++)
        {
            const auto entity = i::m_game_entity_system->get(idx);
            if (!entity)
            {
                // Entity no longer valid - will be cleaned up by TTL
                continue;
            }
            
            auto handle = entity->get_ref_e_handle();
            if (!handle.is_valid())
                continue;
            
            // Check if we already have this entity cached
            auto it = m_cache.find(idx);
            if (it != m_cache.end())
            {
                // Update existing entry
                auto& cached = it->second;
                cached.last_valid_time = now;
                cached.entity_ptr = reinterpret_cast<uintptr_t>(entity);
                cached.serial_number = handle.get_serial_number();
                
                // Re-classify if class name changed (rare but possible)
                auto current_class = entity->get_schema_class_name();
                if (current_class != cached.class_name)
                {
                    cached.class_name = current_class;
                    cached.class_name_hash = fnv1a::hash(current_class);
                    classify_entity(cached);
                }
            }
            else
            {
                // New entity - add to cache
                CachedEntity new_entity;
                new_entity.index = idx;
                new_entity.serial_number = handle.get_serial_number();
                new_entity.entity_ptr = reinterpret_cast<uintptr_t>(entity);
                new_entity.class_name = entity->get_schema_class_name();
                new_entity.class_name_hash = fnv1a::hash(new_entity.class_name);
                new_entity.last_valid_time = now;
                new_entity.is_valid = true;
                
                classify_entity(new_entity);
                m_cache[idx] = std::move(new_entity);
            }
        }
    }
    
    void EntityCache::classify_entity(CachedEntity& entity)
{
    const auto& name = entity.class_name;
    const auto hash = entity.class_name_hash;
    
    // Reset all flags
    entity.type = EntityType::NONE;
    entity.type_mask = static_cast<uint32_t>(EntityType::NONE);
    entity.is_player = false;
    entity.is_projectile = false;
    entity.is_inferno = false;
    entity.is_weapon = false;
    entity.is_planted_c4 = false;
    entity.is_carried_c4 = false;
    
    // METHOD 1: Try to validate as player controller (like get_player_info does)
    // This is more reliable than class name matching
    const auto possible_controller = reinterpret_cast<c_cs_player_controller*>(entity.entity_ptr);
    const auto possible_pawn = possible_controller->get_player_pawn();
    if (possible_pawn)
    {
        const auto health = possible_pawn->m_iHealth();
        const auto team = possible_controller->m_iTeamNum();
        if (health >= 0 && health <= 100 && (team == e_team::t || team == e_team::ct))
        {
            entity.type = EntityType::PLAYER;
            entity.type_mask = static_cast<uint32_t>(EntityType::PLAYER);
            entity.is_player = true;
            return; // Valid player found, no need to check other types
        }
    }
    
    // METHOD 2: Fast classification using class name hash with bitmask assignment
    static const uint32_t hash_player_controller = fnv1a::hash("C_CSPlayerController");
    static const uint32_t hash_player_controller2 = fnv1a::hash("CCSPlayerController");
    static const uint32_t hash_planted_c4 = fnv1a::hash("C_PlantedC4");
    static const uint32_t hash_c4 = fnv1a::hash("C_C4");
    static const uint32_t hash_inferno = fnv1a::hash("C_Inferno");
    /* GRENADES DISABLED - COMMENTED OUT TO FIX FPS LAGS
    static const uint32_t hash_smoke = fnv1a::hash("C_SmokeGrenadeProjectile");
    static const uint32_t hash_molotov = fnv1a::hash("C_MolotovProjectile");
    static const uint32_t hash_flash = fnv1a::hash("C_FlashbangProjectile");
    static const uint32_t hash_he = fnv1a::hash("C_HEGrenadeProjectile");
    static const uint32_t hash_decoy = fnv1a::hash("C_DecoyProjectile");
    */
    
    if (hash == hash_player_controller || hash == hash_player_controller2)
    {
        entity.type = EntityType::PLAYER;
        entity.type_mask = static_cast<uint32_t>(EntityType::PLAYER);
        entity.is_player = true;
    }
    else if (hash == hash_planted_c4)
    {
        entity.type = EntityType::PLANTED_C4;
        entity.type_mask = static_cast<uint32_t>(EntityType::PLANTED_C4);
        entity.is_planted_c4 = true;
    }
    else if (hash == hash_c4)
    {
        entity.type = EntityType::CARRIED_C4;
        entity.type_mask = static_cast<uint32_t>(EntityType::CARRIED_C4);
        entity.is_carried_c4 = true;
    }
    else if (hash == hash_inferno)
    {
        entity.type = EntityType::INFERNO;
        entity.type_mask = static_cast<uint32_t>(EntityType::INFERNO);
        entity.is_inferno = true;
    }
    /* GRENADES DISABLED - COMMENTED OUT TO FIX FPS LAGS
    else if (hash == hash_smoke || hash == hash_molotov || hash == hash_flash || 
             hash == hash_he || hash == hash_decoy)
    {
        entity.type = EntityType::PROJECTILE;
        entity.type_mask = static_cast<uint32_t>(EntityType::PROJECTILE);
        entity.is_projectile = true;
    }
    else if (name.find("Projectile") != std::string::npos)
    {
        entity.type = EntityType::PROJECTILE;
        entity.type_mask = static_cast<uint32_t>(EntityType::PROJECTILE);
        entity.is_projectile = true;
    }
    */
    else if (name.find("Weapon") != std::string::npos || name.find("C_") == 0)
    {
        // Check if it's a dropped weapon (has C_ prefix and contains weapon-related terms)
        if (name.find("Weapon") != std::string::npos || name.find("AK") != std::string::npos ||
            name.find("M4") != std::string::npos || name.find("AWP") != std::string::npos)
        {
            entity.type = EntityType::WEAPON;
            entity.type_mask = static_cast<uint32_t>(EntityType::WEAPON);
            entity.is_weapon = true;
        }
    }
    
    // Debug: track unclassified but interesting entities
    static int unclassified_count = 0;
    if (entity.type == EntityType::NONE)
    {
        if (++unclassified_count <= 20 && !name.empty())  // Log first 20 unclassified
        {
            printf("[DEBUG EntityCache] Unclassified: idx=%d, name='%s', hash=%u\n",
                entity.index, name.c_str(), hash);
        }
    }
}

    std::vector<CachedEntity> EntityCache::get_players() const
    {
        std::vector<CachedEntity> result;
        result.reserve(64);
        
        for (const auto& [idx, entity] : m_cache)
        {
            if (entity.is_player && entity.is_valid)
                result.push_back(entity);
        }
        return result;
    }
    
    /* GRENADES DISABLED - COMMENTED OUT TO FIX FPS LAGS
    std::vector<CachedEntity> EntityCache::get_projectiles() const
    {
        std::vector<CachedEntity> result;
        result.reserve(16);
        
        for (const auto& [idx, entity] : m_cache)
        {
            if (entity.is_projectile && entity.is_valid)
                result.push_back(entity);
        }
        return result;
    }
    */
    
    std::vector<CachedEntity> EntityCache::get_infernos() const
    {
        std::vector<CachedEntity> result;
        result.reserve(8);
        
        for (const auto& [idx, entity] : m_cache)
        {
            if (entity.is_inferno && entity.is_valid)
                result.push_back(entity);
        }
        return result;
    }
    
    std::vector<CachedEntity> EntityCache::get_weapons() const
    {
        std::vector<CachedEntity> result;
        result.reserve(16);
        
        for (const auto& [idx, entity] : m_cache)
        {
            if (entity.is_weapon && entity.is_valid)
                result.push_back(entity);
        }
        return result;
    }
    
    std::vector<CachedEntity> EntityCache::get_all_valid() const
    {
        std::vector<CachedEntity> result;
        result.reserve(m_cache.size());
        
        const auto now = GetTickCount64();
        for (const auto& [idx, entity] : m_cache)
        {
            if (entity.is_valid && (now - entity.last_valid_time) <= CACHE_TTL_MS)
                result.push_back(entity);
        }
        return result;
    }
    
    std::optional<CachedEntity> EntityCache::get_by_index(int32_t idx) const
    {
        auto it = m_cache.find(idx);
        if (it != m_cache.end() && it->second.is_valid)
            return it->second;
        return std::nullopt;
    }
    
    EntityCache::Stats EntityCache::get_stats() const
    {
        Stats stats{};
        stats.total_cached = m_cache.size();
        stats.last_update_ms = m_last_full_scan;
        
        for (const auto& [idx, entity] : m_cache)
        {
            if (entity.is_player) stats.players++;
            if (entity.is_projectile) stats.projectiles++;
            if (entity.is_inferno) stats.infernos++;
        }
        
        return stats;
    }
    
    // Fast bitmask filtering methods
    std::vector<CachedEntity> EntityCache::get_by_mask(EntityType mask) const
    {
        std::vector<CachedEntity> result;
        result.reserve(64);
        
        const uint32_t search_mask = static_cast<uint32_t>(mask);
        
        for (const auto& [idx, entity] : m_cache)
        {
            if (entity.is_valid && (entity.type_mask & search_mask))
            {
                result.push_back(entity);
            }
        }
        return result;
    }
    
    std::vector<CachedEntity> EntityCache::get_players_fast() const
    {
        std::vector<CachedEntity> result;
        result.reserve(64);
        
        const uint32_t player_mask = static_cast<uint32_t>(EntityType::PLAYER);
        
        for (const auto& [idx, entity] : m_cache)
        {
            if (entity.is_valid && (entity.type_mask & player_mask))
            {
                result.push_back(entity);
            }
        }
        return result;
    }
    
    std::vector<CachedEntity> EntityCache::get_weapons_fast() const
    {
        std::vector<CachedEntity> result;
        result.reserve(16);
        
        const uint32_t weapon_mask = static_cast<uint32_t>(EntityType::ALL_WEAPONS);
        
        for (const auto& [idx, entity] : m_cache)
        {
            if (entity.is_valid && (entity.type_mask & weapon_mask))
            {
                result.push_back(entity);
            }
        }
        return result;
    }
    
    std::vector<CachedEntity> EntityCache::get_explosives_fast() const
    {
        std::vector<CachedEntity> result;
        result.reserve(8);
        
        const uint32_t explosives_mask = static_cast<uint32_t>(EntityType::ALL_EXPLOSIVES);
        
        for (const auto& [idx, entity] : m_cache)
        {
            if (entity.is_valid && (entity.type_mask & explosives_mask))
            {
                result.push_back(entity);
            }
        }
        return result;
    }
    
    // Реализация блочного сканирования
    void EntityCache::update_block_based()
    {
        const auto now = GetTickCount64();
        
        // Блок 0-64: Игроки (каждый тик - 60Hz)
        if (now - m_last_player_scan >= PLAYER_SCAN_INTERVAL_MS) {
            scan_entity_range(0, 64, EntityType::PLAYER);
            m_last_player_scan = now;
        }
        
                
        // Блок 512-1024: Оружие (каждые 8 тиков - 7.5Hz)
        if (now - m_last_weapon_scan >= WEAPON_SCAN_INTERVAL_MS) {
            scan_entity_range(512, 1024, EntityType::WEAPON);
            m_last_weapon_scan = now;
        }
    }
    
    void EntityCache::scan_entity_range(int32_t start, int32_t end, EntityType type)
    {
        const uint32_t type_mask = static_cast<uint32_t>(type);
        
        for (int32_t idx = start; idx < end; idx++)
        {
            const auto entity = i::m_game_entity_system->get(idx);
            if (!entity)
                continue;
            
            auto handle = entity->get_ref_e_handle();
            if (!handle.is_valid())
                continue;
            
            // Проверяем, есть ли уже в кэше
            auto it = m_cache.find(idx);
            if (it != m_cache.end())
            {
                // Обновляем существующую запись
                auto& cached = it->second;
                cached.last_valid_time = GetTickCount64();
                cached.entity_ptr = reinterpret_cast<uintptr_t>(entity);
                cached.serial_number = handle.get_serial_number();
                
                // Переопределяем тип если изменился
                auto current_class = entity->get_schema_class_name();
                if (current_class != cached.class_name)
                {
                    cached.class_name = current_class;
                    cached.class_name_hash = fnv1a::hash(current_class);
                    classify_entity(cached);
                }
            }
            else
            {
                // Новая сущность - добавляем в кэш
                CachedEntity new_entity;
                new_entity.index = idx;
                new_entity.serial_number = handle.get_serial_number();
                new_entity.entity_ptr = reinterpret_cast<uintptr_t>(entity);
                new_entity.class_name = entity->get_schema_class_name();
                new_entity.class_name_hash = fnv1a::hash(new_entity.class_name);
                new_entity.last_valid_time = GetTickCount64();
                new_entity.is_valid = true;
                
                classify_entity(new_entity);
                m_cache[idx] = std::move(new_entity);
            }
        }
    }
}
