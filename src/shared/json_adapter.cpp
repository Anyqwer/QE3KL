#include "pch.hpp"
#include "json_adapter.hpp"
#include "utils/skCrypter.h"
#include <cstdlib> // for _itoa_s

namespace shared
{
    // Thread-local reusable JSON object to eliminate heap allocations
    thread_local static nlohmann::json tls_player_json;
    thread_local static char bone_key_buffer[8]; // Buffer for bone key (max 3 digits + null)
    
    nlohmann::json player_to_json(const PlayerData& player, const LocalPlayerData& local)
    {
        // Reuse thread-local JSON object instead of creating new one
        tls_player_json.clear();
        tls_player_json["m_idx"] = player.index;
        tls_player_json["m_name"] = player.name;
        tls_player_json["m_steam_id"] = player.steam_id;
        
        // Team and color
        tls_player_json["m_team"] = player.team;
        tls_player_json["m_color"] = player.color;
        
        // Health
        tls_player_json["m_health"] = player.health;
        tls_player_json["m_is_dead"] = player.is_dead;
        
        // Armor and money
        tls_player_json["m_armor"] = player.armor;
        tls_player_json["m_money"] = player.money;
        tls_player_json["m_has_helmet"] = player.has_helmet;
        tls_player_json["m_has_defuser"] = player.has_defuser;
        
        // World position (for radar)
        tls_player_json["m_position"]["x"] = player.world_pos.m_x;
        tls_player_json["m_position"]["y"] = player.world_pos.m_y;
        tls_player_json["m_position"]["z"] = player.world_pos.m_z;
        
        // Head position (for ESP skeleton)
        tls_player_json["m_head_pos"]["x"] = player.head_pos.m_x;
        tls_player_json["m_head_pos"]["y"] = player.head_pos.m_y;
        tls_player_json["m_head_pos"]["z"] = player.head_pos.m_z;
        
        // Bones (for ESP skeleton) - use fixed array + bitmask
        tls_player_json["m_bones"] = nlohmann::json::object();
        for (int bone_idx = 0; bone_idx < PlayerData::MAX_BONES; ++bone_idx)
        {
            if (player.bone_mask & (1u << bone_idx))
            {
                const auto& bone_pos = player.bone_positions[bone_idx];
                // Fast integer to string conversion using buffer
                _itoa_s(bone_idx, bone_key_buffer, sizeof(bone_key_buffer), 10);
                tls_player_json["m_bones"][bone_key_buffer]["x"] = bone_pos.m_x;
                tls_player_json["m_bones"][bone_key_buffer]["y"] = bone_pos.m_y;
                tls_player_json["m_bones"][bone_key_buffer]["z"] = bone_pos.m_z;
            }
        }
        
        // Rotation/Yaw/Eye angle
        tls_player_json["m_rotation"] = player.yaw;
        tls_player_json["m_yaw"] = player.yaw;
        tls_player_json["m_eye_angle"] = player.eye_angle;
        
        // Weapons
        tls_player_json["m_weapons"]["m_primary"] = player.weapons.primary;
        tls_player_json["m_weapons"]["m_secondary"] = player.weapons.secondary;
        tls_player_json["m_weapons"]["m_active"] = player.weapons.active;
        tls_player_json["m_weapon_name"] = player.weapon_name;
        
        // Status
        tls_player_json["m_is_alive"] = player.is_alive;
        tls_player_json["m_has_c4"] = player.has_c4;
        
        // Legacy weapon name (for compatibility)
        tls_player_json["m_weapon"] = player.weapon_name;
        tls_player_json["m_active_weapon"] = player.weapon_name;
        
        // Legacy is_alive (for compatibility)
        tls_player_json["m_is_alive"] = player.is_alive;
        
        return tls_player_json;
    }
    
    thread_local static nlohmann::json tls_bomb_json;
    
    nlohmann::json bomb_to_json(const BombData& bomb)
    {
        tls_bomb_json.clear();
        
        if (bomb.is_planted || bomb.is_dropped)
        {
            tls_bomb_json["m_position"]["x"] = bomb.position.m_x;
            tls_bomb_json["m_position"]["y"] = bomb.position.m_y;
            tls_bomb_json["m_position"]["z"] = bomb.position.m_z;
        }
        
        if (bomb.is_planted)
        {
            tls_bomb_json["m_state"] = "planted";
            tls_bomb_json["m_blow_time"] = bomb.timer;
            tls_bomb_json["m_is_defused"] = false;
            tls_bomb_json["m_is_defusing"] = bomb.is_defusing;
            tls_bomb_json["m_defuse_time"] = bomb.defuse_timer;
            tls_bomb_json["m_site"] = bomb.site;
        }
        else if (bomb.is_dropped)
        {
            tls_bomb_json["m_state"] = "dropped";
            tls_bomb_json["m_blow_time"] = 0;
        }
        else if (bomb.is_carried)
        {
            tls_bomb_json["m_state"] = "carried";
            tls_bomb_json["m_blow_time"] = 0;
        }
        
        return tls_bomb_json;
    }
    
    thread_local static nlohmann::json tls_state_json;
    
    nlohmann::json game_state_to_json(const GameState& state, const LocalPlayerData& local)
    {
        // Reuse thread-local JSON to eliminate main allocation per frame
        tls_state_json.clear();
        
        // Add map name
        tls_state_json["m_map"] = state.get_map_name();
        
        // Add local player team (for enemy filtering on web radar)
        tls_state_json["m_local_team"] = local.team;
        
        // Add ESP overlay status flag
        tls_state_json["has_esp_overlay"] = true;
        
        // Players array - reuse array instead of creating new
        tls_state_json["m_players"] = nlohmann::json::array();
        auto players = state.get_players();
        for (const auto& player : players)
        {
            if (player.is_alive)
            {
                tls_state_json["m_players"].push_back(player_to_json(player, local));
            }
        }
        
        // Bomb
        tls_state_json["m_bomb"] = bomb_to_json(state.get_bomb());
        
        return tls_state_json;
    }
}
