#pragma once
#include "../../shared/game_data.hpp"

namespace f::bomb
{
	// Scan entity list for C_PlantedC4 (reference-style validation).
	bool find_planted_bomb(shared::BombData& out_data);
	bool find_carried_dropped_bomb(shared::BombData& out_data);
	void get_carried_bomb(c_base_entity* bomb);
	void get_planted_bomb(c_planted_c4* planted_c4);
	bool get_shared_bomb(c_planted_c4* planted_c4, shared::BombData& out_data);
}
