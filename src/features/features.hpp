#pragma once
#include "../shared/game_data.hpp"

namespace f::players
{
	// Original JSON-based function (kept for compatibility)
	bool get_data(int32_t idx, c_cs_player_controller* player, c_cs_player_pawn* player_pawn);
	void get_weapons(c_cs_player_pawn* player_pawn);
	void get_active_weapon(c_cs_player_pawn* player_pawn);
	
	// New function for shared data (for ImGui ESP)
	bool get_shared_data(int32_t idx, c_cs_player_controller* player, c_cs_player_pawn* player_pawn, shared::PlayerData& out_data);
	
	// Cleanup stale cache entries to prevent memory leak (call once per second)
	void cleanup_cache();
}

namespace f::bomb
{
	void get_carried_bomb(c_base_entity* bomb);
	void get_planted_bomb(c_planted_c4* planted_c4);
	
	// New function for shared data
	bool get_shared_bomb(c_planted_c4* planted_c4, shared::BombData& out_data);
}

namespace f
{
	void run();
	void get_map();
	void get_player_info();
	
	// New function to collect all shared data
	void collect_shared_data();

	// Entity cache optimization: scan 8192 once/sec, iterate only active players
	void update_entity_cache(); // Scans 0-8192, fills active_player_indices and planted_c4_index (call once/sec)
	inline std::vector<int32_t> active_player_indices; // Indices of valid players (updated once/sec)
	inline int32_t planted_c4_index = -1; // Index of planted C4 entity (updated once/sec)
	inline int32_t x = -1; // Index of planted C4 entity (updated once/sec)
	inline std::chrono::steady_clock::time_point last_cache_update;

	inline nlohmann::json m_data = {};
	inline nlohmann::json m_player_data = {};
	inline uint32_t m_bomb_idx = 0;
}