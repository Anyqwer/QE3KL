#include "pch.hpp"
#include <unordered_map>
#include <chrono>
#include "../../overlay/menu_config.hpp"

// Static player data cache (name, team, steam_id change rarely)
struct StaticPlayerCache
{
    std::string name;
    int team = 0;
    std::string steam_id;
    std::chrono::steady_clock::time_point last_update;
};
static std::unordered_map<int32_t, StaticPlayerCache> g_static_player_cache;
static constexpr auto STATIC_CACHE_TTL = std::chrono::seconds(5); // Refresh every 5 seconds

// LERP position cache (for smooth ESP interpolation)
struct PositionCache
{
    f_vector prev_pos;
    f_vector current_pos;
    uint64_t last_update_us = 0;
};
static std::unordered_map<int32_t, PositionCache> g_position_cache;
static constexpr auto POSITION_CACHE_TTL = std::chrono::seconds(30); // Cleanup after 30 sec

// Cache cleanup: removes stale entries to prevent memory leak
// Call this periodically (e.g., once per second) from the main loop
static void cleanup_stale_cache_entries()
{
    static auto last_cleanup = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    
    // Only cleanup every 10 seconds to avoid overhead
    if (now - last_cleanup < std::chrono::seconds(10))
        return;
    last_cleanup = now;
    
    // Cleanup static player cache (entries older than 30 seconds)
    for (auto it = g_static_player_cache.begin(); it != g_static_player_cache.end(); )
    {
        if (now - it->second.last_update > std::chrono::seconds(30))
            it = g_static_player_cache.erase(it);
        else
            ++it;
    }
    
    // Cleanup position cache (entries older than 30 seconds)
    auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
    constexpr uint64_t TTL_US = 30'000'000; // 30 seconds in microseconds
    
    for (auto it = g_position_cache.begin(); it != g_position_cache.end(); )
    {
        if (now_us - it->second.last_update_us > TTL_US)
            it = g_position_cache.erase(it);
        else
            ++it;
    }
}

// Public wrapper for cache cleanup
void f::players::cleanup_cache()
{
    cleanup_stale_cache_entries();
}

bool f::players::get_data(int32_t idx, c_cs_player_controller* player, c_cs_player_pawn* player_pawn)
{
	const auto health = player_pawn->m_iHealth();
	const auto is_dead = health <= 0;
	const auto vec_origin = player->get_vec_origin();
	const auto team = player->m_iTeamNum();
	const auto item_services = player_pawn->m_pItemServices();
	const auto money_services = player->m_pInGameMoneyServices();
	const auto eye_angles = player_pawn->m_angEyeAngles();

	m_player_data["m_idx"] = idx;
	m_player_data["m_name"] = player->m_sSanitizedPlayerName();
	m_player_data["m_color"] = player->get_color();
	m_player_data["m_team"] = team;
	m_player_data["m_health"] = health;
	m_player_data["m_armor"] = player_pawn->m_ArmorValue();
	m_player_data["m_money"] = money_services ? money_services->m_iAccount() : 0;
	m_player_data["m_has_helmet"] = item_services ? item_services->m_bHasHelmet() : false;
	m_player_data["m_has_defuser"] = item_services ? item_services->m_bHasDefuser() : false;
	m_player_data["m_yaw"] = eye_angles.m_y;
	m_player_data["m_rotation"] = eye_angles.m_y;
	m_player_data["m_eye_angle"] = eye_angles.m_y;
	m_player_data["m_is_dead"] = is_dead;

	// Removed get_model_name() to prevent string allocation leak
	// m_player_data["m_model_name"] = player_pawn->get_model_name();
	m_player_data["m_steam_id"] = std::to_string(player->m_steamID());
	// ... (���� ��������� ��� ���������� json) ...

	m_player_data["m_position"]["x"] = vec_origin.m_x;
	m_player_data["m_position"]["y"] = vec_origin.m_y;
	m_player_data["m_position"]["z"] = vec_origin.m_z; // �������� Z

		// === Чтение костей игрока ===
	const auto scene_node = player_pawn->m_pGameSceneNode();
	if (scene_node) {
		const auto model_state = reinterpret_cast<uintptr_t>(scene_node) + 0x160;
		const auto bone_array = m_memory->read_t<uintptr_t>(model_state + 0x70);

		if (bone_array) {
			const auto head_pos = m_memory->read_t<f_vector>(bone_array + (6 * 32));
			if (!head_pos.is_zero()) {
				 m_player_data["m_head_pos"]["x"] = head_pos.m_x;
				 m_player_data["m_head_pos"]["y"] = head_pos.m_y;
				 m_player_data["m_head_pos"]["z"] = head_pos.m_z + 8.0f;

				 const int bone_indices[] = {1, 2, 6, 7, 9, 10, 11, 13, 14, 15, 17, 18, 19, 20, 21, 22, 23, 24};
				 for (int i = 0; i < 18; i++) {
					 const auto bone_pos = m_memory->read_t<f_vector>(bone_array + (bone_indices[i] * 32));
					 std::string bone_key = std::to_string(bone_indices[i]);
					 m_player_data["m_bones"][bone_key]["x"] = bone_pos.m_x;
					 m_player_data["m_bones"][bone_key]["y"] = bone_pos.m_y;
					 m_player_data["m_bones"][bone_key]["z"] = bone_pos.m_z;
				 }
			}
		}
	}
	// ============================

	return true;
}

void f::players::get_weapons(c_cs_player_pawn* player_pawn)
{
	const auto weapon_services = player_pawn->m_pWeaponServices();
	if (!weapon_services)
		return;

	const auto my_weapons = weapon_services->m_hMyWeapons();
	if (!my_weapons.m_size)
		return;

	std::set<std::string> utilities_set{};
	std::set<std::string> melee_set{};

	for (size_t idx{ 0 }; idx < my_weapons.m_size; idx++)
	{
		const auto weapon = my_weapons.m_elements->get(idx);
		if (!weapon)
			continue;

		const auto weapon_data = weapon->m_WeaponData();
		if (!weapon_data)
			continue;

		auto weapon_name = weapon_data->m_szName();
		if (weapon_name.empty())
			continue;

		weapon_name.erase(weapon_name.begin(), weapon_name.begin() + 7);

		const auto weapon_type = weapon_data->m_WeaponType();
		switch (weapon_type)
		{
			case e_weapon_type::submachinegun:
			case e_weapon_type::rifle:
			case e_weapon_type::shotgun:
			case e_weapon_type::sniper_rifle:
			case e_weapon_type::machinegun:
				m_player_data["m_weapons"]["m_primary"] = weapon_name;
				break;

			case e_weapon_type::pistol:
				m_player_data["m_weapons"]["m_secondary"] = weapon_name;
				break;

			case e_weapon_type::knife:
			case e_weapon_type::taser:
				melee_set.insert(weapon_name);
				break;

			// case e_weapon_type::grenade:  // DISABLED - not actively used
			// 	utilities_set.insert(weapon_name);
			// 	break;
		}
	}

	m_player_data["m_weapons"]["m_melee"] = std::vector<std::string>(melee_set.begin(), melee_set.end());
	m_player_data["m_weapons"]["m_utilities"] = std::vector<std::string>(utilities_set.begin(), utilities_set.end());

	// Check if player has C4
	bool has_c4 = false;
	for (size_t idx{ 0 }; idx < my_weapons.m_size; idx++)
	{
		const auto weapon = my_weapons.m_elements->get(idx);
		if (!weapon)
			continue;

		const auto weapon_data = weapon->m_WeaponData();
		if (!weapon_data)
			continue;

		auto weapon_name = weapon_data->m_szName();
		if (weapon_name.empty())
			continue;

		// Check if weapon is C4
		if (weapon_name.find("c4") != std::string::npos || weapon_name.find("C4") != std::string::npos)
		{
			has_c4 = true;
			break;
		}
	}
	m_player_data["m_has_c4"] = has_c4;
}

void f::players::get_active_weapon(c_cs_player_pawn* player_pawn)
{
	const auto weapon_services = player_pawn->m_pWeaponServices();
	if (!weapon_services)
		return;

	const auto weapon_handle = weapon_services->m_hActiveWeapon();
	if (!weapon_handle.is_valid())
		return;

	const auto active_weapon = i::m_game_entity_system->get<c_base_player_weapon*>(weapon_handle);
	if (!active_weapon)
		return;

	const auto weapon_data = active_weapon->m_WeaponData();
	if (!weapon_data)
		return;

	auto weapon_name = weapon_data->m_szName();
	if (weapon_name.empty())
		return;

	weapon_name.erase(weapon_name.begin(), weapon_name.begin() + 7);
	m_player_data["m_active_weapon"] = weapon_name;
}

// === BATCH READING STRUCTURES ===
// Layout of C_BaseEntity fields we need (for batch reading)
struct PawnDataBatch
{
    // Offsets from cs2_dumper::schemas::client_dll::C_BaseEntity
    static constexpr size_t OFFSET_HEALTH = 0x34C;
    static constexpr size_t OFFSET_TEAM = 0x3EB;
    static constexpr size_t OFFSET_SCENE_NODE = 0x330;
    static constexpr size_t OFFSET_COLLISION = 0x340;
    static constexpr size_t OFFSET_VELOCITY = 0x3FC;
    static constexpr size_t BUFFER_SIZE = 0x410; // Covers all fields up to velocity + padding
};

struct ExtendedPawnBatch
{
    // C_CSPlayerPawn specific offsets (relative to pawn base)
    static constexpr size_t OFFSET_ARMOR = 0x1C74;
    static constexpr size_t OFFSET_EYE_ANGLES = 0x3300;
    static constexpr size_t OFFSET_SHOTS_FIRED = 0x1C5C;
    static constexpr size_t OFFSET_AIM_PUNCH_SERVICES = 0x1490;
    static constexpr size_t BUFFER_SIZE = 0x3320; // Covers eye angles
};

// Bone matrix batch reading
// Bone array: each bone is 32 bytes (4x4 matrix row or position + padding)
// We read max 30 bones at once to cover all needed indices
static constexpr int MAX_BONES_BATCH = 30;
static constexpr size_t BONE_SIZE = 32;
static constexpr size_t BONE_ARRAY_SIZE = MAX_BONES_BATCH * BONE_SIZE; // 960 bytes

// New function for ImGui ESP - fills shared::PlayerData
// OPTIMIZED: Uses batch reading to reduce syscalls from ~25 to ~3 per player
bool f::players::get_shared_data(int32_t idx, c_cs_player_controller* player, c_cs_player_pawn* player_pawn, shared::PlayerData& out_data)
{
	out_data.index = idx;
	const auto pawn_handle = player_pawn->get_ref_e_handle();
	if (pawn_handle.is_valid())
		out_data.entity_id = pawn_handle.get_entry_idx();
	const uintptr_t pawn_base = reinterpret_cast<uintptr_t>(player_pawn);
	
	// === BATCH 1: Read basic pawn data (health, team, scene_node, collision, velocity) ===
	// Reduces 5 separate syscalls to 1 read of ~1040 bytes
	alignas(16) uint8_t pawn_buffer[PawnDataBatch::BUFFER_SIZE];
	if (!m_memory->read_t(pawn_base, pawn_buffer, PawnDataBatch::BUFFER_SIZE))
		return false;
	
	// Extract values directly from buffer (zero-copy)
	out_data.health = *reinterpret_cast<int32_t*>(pawn_buffer + PawnDataBatch::OFFSET_HEALTH);
	out_data.is_alive = out_data.health > 0;
	out_data.is_dead = !out_data.is_alive;
	
	// Velocity from batch
	const f_vector* velocity_ptr = reinterpret_cast<f_vector*>(pawn_buffer + PawnDataBatch::OFFSET_VELOCITY);
	out_data.velocity = *velocity_ptr;
	
	// Check static cache for team/name/steam_id (updates every 5 seconds)
	auto now = std::chrono::steady_clock::now();
	auto it = g_static_player_cache.find(idx);
	if (it != g_static_player_cache.end() && (now - it->second.last_update) < STATIC_CACHE_TTL)
	{
		out_data.team = it->second.team;
		out_data.name = it->second.name;
		out_data.steam_id = it->second.steam_id;
	}
	else
	{
		// Read from memory and update cache
		StaticPlayerCache cache;
		cache.team = static_cast<int>(*reinterpret_cast<uint8_t*>(pawn_buffer + PawnDataBatch::OFFSET_TEAM));
		cache.name = player->m_sSanitizedPlayerName(); // String still needs separate read
		cache.steam_id = std::to_string(player->m_steamID()); // SteamID still separate
		cache.last_update = now;
		g_static_player_cache[idx] = std::move(cache);
		
		out_data.team = g_static_player_cache[idx].team;
		out_data.name = g_static_player_cache[idx].name;
		out_data.steam_id = g_static_player_cache[idx].steam_id;
	}
	
	// === BATCH 2: Read extended pawn data (armor, angles, shots fired) ===
	// Covers offsets 0x1C00 - 0x3400 in one read (~7KB but saves 3+ syscalls)
	// For performance, we read only critical fields individually for now
	// TODO: Can be optimized further with larger batch
	
	// Armor - individual read (offset 0x1C74, far from base batch)
	out_data.armor = player_pawn->m_ArmorValue();
	
	// Eye angles - individual read (offset 0x3300, very far)
	const auto eye_angles = player_pawn->m_angEyeAngles();
	out_data.yaw = eye_angles.m_y;
	out_data.eye_angle = eye_angles.m_y;
	
	// RCS data - shots fired
	out_data.shots_fired = player_pawn->m_iShotsFired();
	out_data.is_scoped = player_pawn->m_bIsScoped();
	
	// Aim punch - still needs pointer chase (services pointer)
	const auto aim_punch_services = player_pawn->m_pAimPunchServices();
	if (aim_punch_services && reinterpret_cast<uintptr_t>(aim_punch_services) > 0x1000)
	{
		const auto punch_angle = aim_punch_services->m_predictableBaseAngle();
		out_data.aim_punch_angle = punch_angle;
	}
	else
	{
		out_data.aim_punch_angle = { 0.0f, 0.0f, 0.0f };
	}
	
	// Collision bounds - read from batch (pointer is in buffer, but need to read collision struct)
	// CCollisionProperty: m_vecMins = 0x40, m_vecMaxs = 0x4C (from cs2_dumper)
	const auto collision_ptr = *reinterpret_cast<uintptr_t*>(pawn_buffer + PawnDataBatch::OFFSET_COLLISION);
	if (collision_ptr > 0x1000)
	{
		// Read collision bounds: mins at +0x40, maxs at +0x4C (total span ~24 bytes with padding)
		alignas(16) uint8_t collision_buffer[64];
		if (m_memory->read_t(collision_ptr + 0x40, collision_buffer, 24)) // m_vecMins at +0x40, m_vecMaxs at +0x4C
		{
			out_data.mins = *reinterpret_cast<f_vector*>(collision_buffer);
			out_data.maxs = *reinterpret_cast<f_vector*>(collision_buffer + 0x0C); // 0x4C - 0x40 = 0x0C = 12 bytes
		}
		else
		{
			out_data.mins = { -16.0f, -16.0f, 0.0f };
			out_data.maxs = { 16.0f, 16.0f, 72.0f };
		}
	}
	else
	{
		out_data.mins = { -16.0f, -16.0f, 0.0f };
		out_data.maxs = { 16.0f, 16.0f, 72.0f };
	}
	
	// Player color (still needs separate read, cached rarely changes)
	out_data.color = static_cast<int>(player->get_color());
	
	// Money services - separate read
	const auto money_services = player->m_pInGameMoneyServices();
	out_data.money = money_services ? money_services->m_iAccount() : 0;
	
	// Item services (helmet, defuser) - separate read
	const auto item_services = player_pawn->m_pItemServices();
	out_data.has_helmet = item_services ? item_services->m_bHasHelmet() : false;
	out_data.has_defuser = item_services ? item_services->m_bHasDefuser() : false;
	
	// Weapons
	const auto weapon_services = player_pawn->m_pWeaponServices();
	if (weapon_services)
	{
		const auto my_weapons = weapon_services->m_hMyWeapons();
		std::set<std::string> utilities_set{};
		std::set<std::string> melee_set{};
		
		for (size_t i{ 0 }; i < my_weapons.m_size; i++)
		{
			const auto weapon = my_weapons.m_elements->get(i);
			if (!weapon) continue;
			const auto weapon_data = weapon->m_WeaponData();
			if (!weapon_data) continue;
			
			auto weapon_name = weapon_data->m_szName();
			if (weapon_name.empty()) continue;
			
			weapon_name.erase(weapon_name.begin(), weapon_name.begin() + 7);
			
			const auto weapon_type = weapon_data->m_WeaponType();
			switch (weapon_type)
			{
				case e_weapon_type::submachinegun:
				case e_weapon_type::rifle:
				case e_weapon_type::shotgun:
				case e_weapon_type::sniper_rifle:
				case e_weapon_type::machinegun:
					out_data.weapons.primary = weapon_name;
					break;
					
				case e_weapon_type::pistol:
					out_data.weapons.secondary = weapon_name;
					break;
					
				case e_weapon_type::knife:
				case e_weapon_type::taser:
					melee_set.insert(weapon_name);
					break;
					
				// case e_weapon_type::grenade:  // DISABLED - not actively used
				// 	utilities_set.insert(weapon_name);
				// 	break;
			}
			
			// Check for C4
			if (weapon_name.find("c4") != std::string::npos || weapon_name.find("C4") != std::string::npos)
			{
				out_data.has_c4 = true;
			}
		}
		
		out_data.weapons.melee = std::vector<std::string>(melee_set.begin(), melee_set.end());
		out_data.weapons.utilities = std::vector<std::string>(utilities_set.begin(), utilities_set.end());
		
		// Get active weapon
		const auto weapon_handle = weapon_services->m_hActiveWeapon();
		if (weapon_handle.is_valid())
		{
			const auto active_weapon = i::m_game_entity_system->get<c_base_player_weapon*>(weapon_handle);
			if (active_weapon)
			{
				const auto weapon_data = active_weapon->m_WeaponData();
				if (weapon_data)
				{
					auto name = weapon_data->m_szName();
					if (!name.empty() && name.length() > 7)
					{
						name.erase(name.begin(), name.begin() + 7);
						out_data.weapons.active = name;
						out_data.weapon_name = name;
					}
				}
			}
		}
	}
	
	// === POSITION READING ===
	// Get scene_node pointer from batch (already read in pawn_buffer)
	const uintptr_t scene_node = *reinterpret_cast<uintptr_t*>(pawn_buffer + PawnDataBatch::OFFSET_SCENE_NODE);
	
	if (scene_node > 0x1000)
	{
		// Read m_vecAbsOrigin from CGameSceneNode (offset 0xC8, VectorWS)
		// This gives accurate world position without network quantization
		f_vector origin{};
		if (m_memory->read_t(scene_node + 0xC8, &origin, sizeof(f_vector)))
		{
			out_data.world_pos = origin;
		}
		else
		{
			out_data.world_pos = {};
		}
	}
	else
	{
		out_data.world_pos = {};
	}
	
		// === BATCH BONE READING (CRITICAL FOR ESP SYNC) ===
	// Only read bone matrix if ESP is enabled AND we have valid scene_node
	if (esp::g_menu_config.ESPEnabled && scene_node > 0x1000)
	{
		// Read model_state -> bone_array pointer in one go
		// model_state is at scene_node + 0x160 (CSkeletonInstance::m_modelState)
		// bone_array pointer is at model_state + 0x80
		uintptr_t bone_array_ptr = 0;
		if (m_memory->read_t(scene_node + 0x160 + 0x70, &bone_array_ptr, sizeof(uintptr_t)) && bone_array_ptr > 0x1000)
		{
			// === BATCH READ ALL BONES AT ONCE ===
			// 30 bones * 32 bytes = 960 bytes (less than 1KB, single syscall)
			alignas(16) uint8_t bone_batch[BONE_ARRAY_SIZE];
			
			if (m_memory->read_t(bone_array_ptr, bone_batch, BONE_ARRAY_SIZE))
			{
				// Extract head position (bone 6) from batch
				const f_vector* head_bone = reinterpret_cast<f_vector*>(bone_batch + (6 * BONE_SIZE));
				if (!head_bone->is_zero())
				{
					out_data.head_pos.m_x = head_bone->m_x;
					out_data.head_pos.m_y = head_bone->m_y;
					out_data.head_pos.m_z = head_bone->m_z + 8.0f;
					
					// Only read full skeleton if ShowSkeleton is enabled
					if (esp::g_menu_config.ShowSkeleton)
					{
						// Clear bone mask and positions
						out_data.bone_mask = 0;
						out_data.bone_positions.fill(vector_t(0, 0, 0));
						
						// Bone indices we need for skeleton - INCLUDING bone 24 for spine
						static const int bone_indices[] = {1, 2, 6, 7, 9, 10, 11, 13, 14, 15, 17, 18, 19, 20, 21, 22, 23, 24};
						static constexpr int num_bones = sizeof(bone_indices) / sizeof(bone_indices[0]);
						
						// Extract from batch using FIXED ARRAY (zero hash overhead!)
						for (int i = 0; i < num_bones; i++)
						{
							int bone_idx = bone_indices[i];
							if (bone_idx >= 0 && bone_idx < shared::PlayerData::MAX_BONES)
							{
								const f_vector* bone_ptr = reinterpret_cast<f_vector*>(bone_batch + (bone_idx * BONE_SIZE));
								out_data.bone_positions[bone_idx] = *bone_ptr;
								out_data.bone_mask |= (1u << bone_idx);  // Mark bone as valid
							}
						}
					}
				}
				else
				{
					out_data.bone_mask = 0;
					out_data.bone_positions.fill(vector_t(0, 0, 0));
				}
			}
			else
			{
				out_data.bone_mask = 0;
				out_data.bone_positions.fill(vector_t(0, 0, 0));
			}
		}
		else
		{
			out_data.bone_mask = 0;
			out_data.bone_positions.fill(vector_t(0, 0, 0));
		}
	}
	else
	{
		// ESP disabled: clear bones, keep position for radar
		out_data.bone_mask = 0;
		out_data.bone_positions.fill(vector_t(0, 0, 0));
	}
	
	// === LERP POSITION UPDATE (for smooth ESP) ===
	// Save current position to cache and update LERP fields
	auto now_us = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
	
	auto& pos_cache = g_position_cache[idx];
	out_data.prev_world_pos = pos_cache.current_pos;  // Previous becomes prev_world_pos
	out_data.memory_update_time = static_cast<uint64_t>(now_us);
	
	// Update cache for next frame
	pos_cache.prev_pos = pos_cache.current_pos;
	pos_cache.current_pos = out_data.world_pos;
	pos_cache.last_update_us = static_cast<uint64_t>(now_us);
	
	return out_data.is_alive;
}