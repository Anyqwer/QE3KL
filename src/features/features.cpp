#include "pch.hpp"
#include <algorithm>
#include "../shared/game_data.hpp"
#include "../shared/view.hpp"
#include "../shared/json_adapter.hpp"
#include "../core/entity_cache.hpp"
#include "../core/simple_optimized_cache.hpp"
#include "../utils/skCrypter.h"
#include "bomb/bomb.hpp"
#include "triggerbot/triggerbot.hpp"
#include <unordered_map>

namespace
{
	bool crosshair_matches_player(int crosshair_id, const shared::PlayerData& player)
	{
		if (crosshair_id <= 0)
			return false;

		return player.entity_id == crosshair_id ||
			player.index == crosshair_id;
	}

	void process_hitmarkers(const shared::LocalPlayerData& local, const std::vector<shared::PlayerData>& players)
	{
		static std::unordered_map<int, int> prev_hp;
		static int last_crosshair_id = -1;
		static int32_t last_shots_fired = 0;
		static auto last_shot_time = std::chrono::steady_clock::now();

		if (local.crosshair_id > 0)
			last_crosshair_id = local.crosshair_id;

		if (local.shots_fired > last_shots_fired)
		{
			last_shots_fired = local.shots_fired;
			last_shot_time = std::chrono::steady_clock::now();
		}

		const auto ms_since_shot = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now() - last_shot_time).count();

		for (const auto& player : players)
		{
			if (player.team == local.team)
			{
				prev_hp[player.index] = player.health;
				continue;
			}

			const auto it = prev_hp.find(player.index);
			if (it == prev_hp.end())
			{
				prev_hp[player.index] = player.health;
				continue;
			}

			const int previous_hp = it->second;
			const bool took_damage = player.health < previous_hp && !player.is_dead;
			const bool got_kill = !player.is_dead && previous_hp > 0 && player.is_dead;

			if (took_damage || got_kill)
			{
				const bool crosshair_now = crosshair_matches_player(local.crosshair_id, player);
				const bool crosshair_recent = crosshair_matches_player(last_crosshair_id, player);
				const bool shot_recent = ms_since_shot >= 0 && ms_since_shot < 450;

				if (crosshair_now || (shot_recent && crosshair_recent) || got_kill && crosshair_recent)
				{
					const bool is_kill = got_kill || player.health <= 0;
					shared::g_hitmarker_bus.push(is_kill);
				}
			}

			prev_hp[player.index] = player.health;
		}
	}

    void log_bomb_status(const shared::BombData& out_data)
    {
        static auto last_log_time = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time).count() >= 3)
        {
            std::string state = "none";
            if (out_data.is_planted) state = "planted";
            else if (out_data.is_carried) state = "carried";
            else if (out_data.is_dropped) state = "dropped";

            printf("[BOMB LOG] State: %s | Pos: (%.1f, %.1f, %.1f) | Timer: %.1f\n",
                state.c_str(), out_data.position.m_x, out_data.position.m_y, out_data.position.m_z, out_data.timer);
            last_log_time = now;
        }
    }
}

// === OPTIMIZED Entity Cache Update ===
// Replaces old 0-4096 full scan with bitmap-based scanning
// Performance: 75% reduction in RPM calls (1024 -> 128 average)
void f::update_entity_cache()
{
	// Use simple optimized cache - call every frame for best performance
	core::g_simple_cache.update();
	
	// Update active_player_indices for compatibility with existing code
	active_player_indices = core::g_simple_cache.get_player_indices();
	
	// Update cache timestamp for compatibility
	last_cache_update = std::chrono::steady_clock::now();
}

void f::run()
{
	// Update optimized entity cache (bitmap-based scanning)
	update_entity_cache();
	
	// Cleanup player caches to prevent memory leak (entries > 30 sec removed)
	f::players::cleanup_cache();
	
	// Debug: check local controller
	if (!sdk::m_local_controller)
	{
		static bool logged = false;
		if (!logged)
		{
			printf("[DEBUG ERROR] sdk::m_local_controller is null!\n");
			logged = true;
		}
		return;
	}
	
	const auto local_team = sdk::m_local_controller->m_iTeamNum();
	if (local_team == e_team::none)
	{
		static bool logged = false;
		if (!logged)
		{
			printf("[DEBUG] Local team is NONE, waiting for game start...\n");
			logged = true;
		}
		return;
	}

	m_data = nlohmann::json{};
	m_player_data = nlohmann::json{};

	m_data["m_local_team"] = local_team;

	// === ===
	if (sdk::m_local_controller)
	{
		const auto local_pawn = sdk::m_local_controller->get_player_pawn();
		if (local_pawn)
		{
			const auto local_origin = local_pawn->get_scene_origin();
			const auto local_angles = local_pawn->m_angEyeAngles();

			m_data["m_local_position"]["x"] = local_origin.m_x;
			m_data["m_local_position"]["y"] = local_origin.m_y;
			m_data["m_local_position"]["z"] = local_origin.m_z;
			m_data["m_local_yaw"] = local_angles.m_y;
			m_data["m_local_rotation"] = local_angles.m_y;
		}
	}

	// ===      ===
	//      	// В функции get_module_info().first
	const auto client_base = m_memory->get_module_info(CLIENT_DLL).first.value_or(0);

	get_map();
	get_player_info();

	const auto bomb = shared::g_game_state.get_bomb();
	if (bomb.is_planted)
		m_data["m_bomb"] = shared::bomb_to_json(bomb);
}

void f::get_map()
{
	static std::string last_valid_map = "menu";
	if (!i::m_global_vars)
	{
		m_data["m_map"] = last_valid_map;
		return;
	}

	const auto map_name = i::m_global_vars->m_map_name();
	if (map_name.empty() || map_name.find("<empty>") != std::string::npos)
	{
		m_data["m_map"] = last_valid_map;
		return;
	}

	last_valid_map = map_name;
	m_data["m_map"] = map_name;
}

void f::get_player_info()
{
    m_data["m_players"] = nlohmann::json::array();

    int32_t players_pushed = 0;
    
    // 2. СКАНИРОВАНИЕ ИГРОКОВ И БОМБЫ В РУКАХ
    for (int32_t idx : active_player_indices)
    {
        try
        {
            const auto entity = i::m_game_entity_system->get(idx);
            if (!entity) continue;

            const auto controller = reinterpret_cast<c_cs_player_controller*>(entity);
            const auto pawn = controller->get_player_pawn();
            if (!pawn) continue;

            const auto health = pawn->m_iHealth();
            const auto team = controller->m_iTeamNum();
            if (health >= 0 && health <= 100 && (team == e_team::t || team == e_team::ct))
            {
                if (f::players::get_data(idx, controller, pawn))
                {
                    f::players::get_weapons(pawn);
                    f::players::get_active_weapon(pawn);

                    m_data["m_players"].push_back(m_player_data);
                    players_pushed++;
                }
            }
        }
        catch (...) { continue; }
    }
}

// New function: collect shared data for ImGui ESP
void f::collect_shared_data()

{
	std::vector<shared::PlayerData> players;
	shared::LocalPlayerData local{};
	
	// Get local player data
	if (sdk::m_local_controller)
	{
		const auto local_pawn = sdk::m_local_controller->get_player_pawn();
		if (local_pawn)
		{
			local.is_valid = true;
			local.team = static_cast<int>(sdk::m_local_controller->m_iTeamNum());
			local.position = local_pawn->get_scene_origin();
			local.view_angles = local_pawn->m_angEyeAngles();
			local.fov = 90.0f;

			// Read shots fired (m_iShotsFired) for auto-swap
			local.shots_fired = local_pawn->m_iShotsFired();

			// Store local pawn pointer for bhop
			local.local_pawn = local_pawn;

			// Get active weapon name for auto-swap
			const auto weapon_services = local_pawn->m_pWeaponServices();
			if (weapon_services)
			{
				const auto weapon_handle = weapon_services->m_hActiveWeapon();
				if (weapon_handle.is_valid())
				{
					const auto active_weapon = i::m_game_entity_system->get<c_base_player_weapon*>(weapon_handle);
					if (active_weapon)
					{
						const auto weapon_data = active_weapon->m_WeaponData();
						if (weapon_data)
						{
							auto weapon_name = weapon_data->m_szName();
							if (!weapon_name.empty() && weapon_name.length() > 7)
							{
								weapon_name.erase(weapon_name.begin(), weapon_name.begin() + 7);
								local.weapon_name = weapon_name;
							}
						}
					}
				}
			}

			// Read Crosshair ID (m_iIDEntIndex) for Triggerbot
			local.crosshair_id = m_memory->read_t<int>(reinterpret_cast<uintptr_t>(local_pawn) + g_offsets::m_iIDEntIndex);

			// Read view matrix for WorldToScreen (needed for Triggerbot)
			static uintptr_t client_base_local = 0;
			if (client_base_local == 0)
			{
				auto module_info = m_memory->get_module_info(CLIENT_DLL);
				if (module_info.first.has_value())
					client_base_local = module_info.first.value();
			}
			if (client_base_local != 0)
			{
				struct view_matrix_t { float matrix[16]; };
				auto vm = m_memory->read_t<view_matrix_t>(client_base_local + g_offsets::view_matrix);
				std::copy(std::begin(vm.matrix), std::end(vm.matrix), local.view_matrix.begin());
			}
		}
	}
	
	// === OPTIMIZED Player Data Collection ===
	// Use simple optimized cache for ultra-fast player retrieval
	// Performance: Direct bitmap access instead of linear search
	auto player_indices = core::g_simple_cache.get_player_indices();
	
		for (int32_t idx : player_indices)
	{
		try
		{
			const auto entity = i::m_game_entity_system->get(idx);
			if (!entity)
				continue;
			
			const auto controller = reinterpret_cast<c_cs_player_controller*>(entity);
			const auto pawn = controller->get_player_pawn();
			if (!pawn)
				continue;
			
			const auto health = pawn->m_iHealth();
            bool is_dead = (health <= 0 || health > 100);
			
			// Fill shared data
			shared::PlayerData player_data;
			if (f::players::get_shared_data(idx, controller, pawn, player_data))
			{
                player_data.health = health;
                player_data.is_dead = is_dead;
				player_data.is_on_screen = false;  // Will be calculated in overlay
				
				players.push_back(player_data);
			}
		}
		catch (...)
		{
			continue;
		}
	}
		
	// Get map name for shared state
	static std::string last_valid_map = "menu";
	std::string current_map = "menu";
	if (i::m_global_vars)
	{
		const auto map_name = i::m_global_vars->m_map_name();
		if (!map_name.empty() && map_name.find("<empty>") == std::string::npos)
		{
			last_valid_map = map_name;
			current_map = map_name;
		}
		else
		{
			current_map = last_valid_map;
		}
	}
	
	shared::BombData bomb{};
	f::bomb::find_carried_dropped_bomb(bomb);

	log_bomb_status(bomb);

	process_hitmarkers(local, players);

	// Update shared game state (legacy for WebSocket)
	shared::g_game_state.update_local(local);
	shared::g_game_state.update_players(players);
	shared::g_game_state.update_bomb(bomb);
	shared::g_game_state.update_map(current_map);
	// Update double-buffered state for ImGui ESP (lock-free)
	shared::g_double_buffered_state.write_local(local);
	shared::g_double_buffered_state.write_players(players);
	shared::g_double_buffered_state.write_bomb(bomb);
	shared::g_double_buffered_state.write_map(current_map);
	
	// Project player positions to screen for Triggerbot BEFORE swap
	// Get screen dimensions
	int screen_w = GetSystemMetrics(SM_CXSCREEN);
	int screen_h = GetSystemMetrics(SM_CYSCREEN);

	for (auto& player : players)
	{
		vector_t screen_pos;
		player.is_on_screen = shared::world_to_screen(
			player.head_pos,  // Aim at head
			screen_pos,
			local.view_matrix,
			static_cast<float>(screen_w),
			static_cast<float>(screen_h)
		);
		player.screen_pos = screen_pos;
	}
	
	// Swap buffers for next frame
	shared::g_double_buffered_state.swap_buffers();
	
	// Run Triggerbot (after swap, using the data we just wrote)
	f::triggerbot::run(local, players);
}