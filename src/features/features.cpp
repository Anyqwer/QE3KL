#include "pch.hpp"
#include <algorithm>
#include "../shared/game_data.hpp"
#include "../shared/view.hpp"
#include "../shared/json_adapter.hpp"
#include "../core/entity_cache.hpp"
#include "../core/simple_optimized_cache.hpp"
#include "../utils/skCrypter.h"
#include "triggerbot/triggerbot.hpp"

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

	// === ������ ������� ���� ��� �� ===
	// � ����� ���� ����� ������ �������	// В функции get_module_info().first
	const auto client_base = m_memory->get_module_info(CLIENT_DLL).first.value_or(0);

	get_map();
	get_player_info();
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
			if (health <= 0 || health > 100)
				continue;
			
			// Fill shared data
			shared::PlayerData player_data;
			if (f::players::get_shared_data(idx, controller, pawn, player_data))
			{
				// Memory thread only stores 3D world data
				// Projection to screen coordinates is done in Render Thread
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
	
	// Update shared game state (legacy for WebSocket)
	shared::g_game_state.update_local(local);
	shared::g_game_state.update_players(players);
	shared::g_game_state.update_map(current_map);
	// Update double-buffered state for ImGui ESP (lock-free)
	shared::g_double_buffered_state.write_local(local);
	shared::g_double_buffered_state.write_players(players);
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