#pragma once
#include "game_data.hpp"
#include "../ext/nlohmann/json.hpp"

namespace shared
{
    // Convert shared game state to JSON for WebRadar
    nlohmann::json game_state_to_json(const GameState& state, const LocalPlayerData& local);
    
    // Convert single player to JSON
    nlohmann::json player_to_json(const PlayerData& player, const LocalPlayerData& local);
    
    // Convert bomb to JSON
    nlohmann::json bomb_to_json(const BombData& bomb);
}
