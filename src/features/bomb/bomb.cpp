#include "pch.hpp"

void f::bomb::get_carried_bomb(c_base_entity* bomb)
{
	const auto scene_node = bomb->m_pGameSceneNode();
	if (!scene_node)
	{
		return;
	}

	const auto vec_origin = scene_node->m_vecAbsOrigin();

	m_data["m_bomb"]["m_position"]["x"] = vec_origin.m_x;
	m_data["m_bomb"]["m_position"]["y"] = vec_origin.m_y;
	m_data["m_bomb"]["m_position"]["z"] = vec_origin.m_z;
}

void f::bomb::get_planted_bomb(c_planted_c4* planted_c4)
{
	if (!planted_c4->m_bBombTicking())
		return;

	const auto curtime = i::m_global_vars->m_curtime();

	const auto blow_time = (planted_c4->m_flC4Blow() - curtime);
	if (blow_time <= 0.f)
		return;

	const auto vec_origin = planted_c4->m_pGameSceneNode()->m_vecAbsOrigin();
	if (vec_origin.is_zero())
		return;

	const auto is_defused = planted_c4->m_bBombDefused();
	const auto is_defusing = planted_c4->m_bBeingDefused();
	const auto defuse_time = (planted_c4->m_flDefuseCountDown() - curtime);

	m_data["m_bomb"]["m_position"]["x"] = vec_origin.m_x;
	m_data["m_bomb"]["m_position"]["y"] = vec_origin.m_y;
	m_data["m_bomb"]["m_position"]["z"] = vec_origin.m_z;
	m_data["m_bomb"]["m_blow_time"] = blow_time;
	m_data["m_bomb"]["m_is_defused"] = is_defused;
	m_data["m_bomb"]["m_is_defusing"] = is_defusing;
	m_data["m_bomb"]["m_defuse_time"] = defuse_time;
}

// New function for ImGui ESP - fills shared::BombData
bool f::bomb::get_shared_bomb(c_planted_c4* planted_c4, shared::BombData& out_data)
{
	if (!planted_c4->m_bBombTicking())
		return false;
	
	const auto curtime = i::m_global_vars->m_curtime();
	const auto blow_time = (planted_c4->m_flC4Blow() - curtime);
	if (blow_time <= 0.f)
		return false;
	
	const auto vec_origin = planted_c4->m_pGameSceneNode()->m_vecAbsOrigin();
	if (vec_origin.is_zero())
		return false;
	
	// Get defuse status
	const auto is_defusing = planted_c4->m_bBeingDefused();
	const auto defuse_countdown = planted_c4->m_flDefuseCountDown();
	const auto defuse_time = (defuse_countdown - curtime);
	
	// Can CT defuse in time?
	bool can_defuse = false;
	if (is_defusing && defuse_time > 0)
	{
		can_defuse = defuse_time < blow_time;  // Defuse will complete before explosion
	}
	
	out_data.is_planted = true;
	out_data.position = vec_origin;
	out_data.timer = blow_time;
	out_data.site = 0;  // Default to Site A (m_nBombSite not available in SDK)
	out_data.is_defusing = is_defusing;
	out_data.defuse_timer = defuse_time > 0 ? defuse_time : 0;
	out_data.can_defuse = can_defuse;
	
	return true;
}