#include "pch.hpp"
#include "bomb.hpp"
#include "../features.hpp"
#include "../../utils/skCrypter.h"
#include <chrono>


namespace
{
	static constexpr fnv1a_t k_planted_c4_hash = fnv1a::hash_const("C_PlantedC4");

	bool is_planted_c4_entity(c_base_entity* entity)
	{
		if (!entity)
			return false;

		const auto class_name = entity->get_schema_class_name();
		if (class_name == "C_PlantedC4")
			return true;

		return fnv1a::hash(class_name) == k_planted_c4_hash;
	}

	bool try_read_planted(c_planted_c4* planted, shared::BombData& out_data, int32_t entity_index)
	{
		if (!planted || !i::m_global_vars)
			return false;

		const float curtime = i::m_global_vars->m_curtime();

		if (planted->m_bBombDefused())
			return false;

		const float explode_time = planted->m_flC4Blow();
		if (explode_time <= curtime)
			return false;

		const auto scene_node = planted->m_pGameSceneNode();
		if (!scene_node)
			return false;

		const auto vec_origin = scene_node->m_vecAbsOrigin();
		if (vec_origin.is_zero())
			return false;

		const auto is_defusing = planted->m_bBeingDefused();
		const auto defuse_countdown = planted->m_flDefuseCountDown();
		const float defuse_time = defuse_countdown - curtime;
		const float blow_time = explode_time - curtime;

		bool can_defuse = false;
		if (is_defusing && defuse_time > 0.f)
			can_defuse = defuse_time < blow_time;

		out_data = {};
		out_data.is_planted = true;
		out_data.position = vec_origin;
		out_data.timer = blow_time;
		out_data.site = planted->m_nBombSite();
		out_data.is_defusing = is_defusing;
		out_data.defuse_timer = defuse_time > 0.f ? defuse_time : 0.f;
		out_data.can_defuse = can_defuse;

		if (entity_index >= 0)
			f::planted_c4_index = entity_index;

		return true;
	}

	bool try_planted_pointer(uintptr_t planted_ptr, shared::BombData& out_data, int32_t entity_index)
	{
		if (!planted_ptr || planted_ptr < 0x10000)
			return false;

		return try_read_planted(reinterpret_cast<c_planted_c4*>(planted_ptr), out_data, entity_index);
	}

	bool try_from_dw_planted_c4(shared::BombData& out_data)
	{
		if (!g_offsets::planted_c4)
			g_offsets::planted_c4 = cs2_dumper::offsets::client_dll::dwPlantedC4;

		const auto client_base = m_memory->get_module_info(CLIENT_DLL).first.value_or(0);
		if (!client_base)
			return false;

		const auto planted_addr = client_base + g_offsets::planted_c4;

		// cs2-dumper: global pointer to C_PlantedC4
		const auto planted_ptr = m_memory->read_t<uintptr_t>(planted_addr);
		if (try_planted_pointer(planted_ptr, out_data, -1))
			return true;

		// fallback: some builds store an extra indirection
		if (planted_ptr > 0x10000)
		{
			const auto indirect_ptr = m_memory->read_t<uintptr_t>(planted_ptr);
			if (try_planted_pointer(indirect_ptr, out_data, -1))
				return true;
		}

		return false;
	}

	int32_t get_highest_entity_index()
	{
		if (!i::m_game_entity_system)
			return 1024;

		const auto entity_system = reinterpret_cast<uintptr_t>(i::m_game_entity_system);
		const auto highest = m_memory->read_t<int32_t>(
			entity_system + cs2_dumper::offsets::client_dll::dwGameEntitySystem_highestEntityIndex);

		if (highest <= 0 || highest > 8192)
			return 1024;

		return highest + 1;
	}

	bool scan_entity_list_for_planted(shared::BombData& out_data)
	{
		if (!i::m_game_entity_system)
			return false;

		const int32_t scan_end = get_highest_entity_index();

		for (int32_t idx = 0; idx < scan_end; ++idx)
		{
			const auto entity = i::m_game_entity_system->get(idx);
			if (!entity || !is_planted_c4_entity(entity))
				continue;

			auto* planted = reinterpret_cast<c_planted_c4*>(entity);
			if (try_read_planted(planted, out_data, idx))
				return true;
		}

		return false;
	}

    void log_bomb_status(const shared::BombData& out_data)
    {
        static auto last_log_time = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time).count() >= 3)
        {
            std::string state = "unknown";
            if (out_data.is_planted) state = "planted";
            else if (out_data.is_carried) state = "carried";
            else if (out_data.is_dropped) state = "dropped";

            printf("[BOMB LOG] State: %s | Pos: (%.1f, %.1f, %.1f) | Timer: %.1f\n",
                state.c_str(),
                out_data.position.m_x,
                out_data.position.m_y,
                out_data.position.m_z,
                out_data.timer);

            last_log_time = now;
        }
    }
}

bool f::bomb::find_planted_bomb(shared::BombData& out_data)
{
	out_data = {};
	f::planted_c4_index = -1;

	if (!i::m_global_vars)
		return false;

	if (try_from_dw_planted_c4(out_data))
		return true;

	return scan_entity_list_for_planted(out_data);
}

void f::bomb::get_carried_bomb(c_base_entity* bomb)
{
	const auto scene_node = bomb->m_pGameSceneNode();
	if (!scene_node)
		return;

	const auto vec_origin = scene_node->m_vecAbsOrigin();

	m_data["m_bomb"]["m_position"]["x"] = vec_origin.m_x;
	m_data["m_bomb"]["m_position"]["y"] = vec_origin.m_y;
	m_data["m_bomb"]["m_position"]["z"] = vec_origin.m_z;
}

void f::bomb::get_planted_bomb(c_planted_c4* planted_c4)
{
	shared::BombData bomb{};
	if (!get_shared_bomb(planted_c4, bomb))
		return;

	m_data["m_bomb"]["m_position"]["x"] = bomb.position.m_x;
	m_data["m_bomb"]["m_position"]["y"] = bomb.position.m_y;
	m_data["m_bomb"]["m_position"]["z"] = bomb.position.m_z;
	m_data["m_bomb"]["m_blow_time"] = bomb.timer;
	m_data["m_bomb"]["m_is_defused"] = false;
	m_data["m_bomb"]["m_is_defusing"] = bomb.is_defusing;
	m_data["m_bomb"]["m_defuse_time"] = bomb.defuse_timer;
	m_data["m_bomb"]["m_site"] = bomb.site;
}

bool f::bomb::get_shared_bomb(c_planted_c4* planted_c4, shared::BombData& out_data)
{
	return try_read_planted(planted_c4, out_data, -1);
}

bool f::bomb::find_carried_dropped_bomb(shared::BombData& out_data)
{
    if (find_planted_bomb(out_data)) return true;

    if (!i::m_game_entity_system) return false;

    // Читаем highest_index напрямую через память
	int highest_index = m_memory->read_t<int>(reinterpret_cast<uintptr_t>(i::m_game_entity_system) + cs2_dumper::offsets::client_dll::dwGameEntitySystem_highestEntityIndex);
	if (highest_index > 2048) highest_index = 2048;

	for (int i = 65; i <= highest_index; ++i)
	{
		uintptr_t list_entry = m_memory->read_t<uintptr_t>(reinterpret_cast<uintptr_t>(i::m_game_entity_system) + 8 * ((i & 0x7FFF) >> 9) + 16);
		if (!list_entry) continue;
		uintptr_t entity = m_memory->read_t<uintptr_t>(list_entry + 120 * (i & 0x1FF));
		if (!entity) continue;

		std::string class_name = reinterpret_cast<c_base_entity*>(entity)->get_schema_class_name();

        // ЛОГЕР (временно)
        if (class_name.find("C4") != std::string::npos) {
             printf("[DEBUG] Found entity %d: %s\n", i, class_name.c_str());
        }

        if (class_name.find("C_WeaponC4") != std::string::npos)
		{
            auto* weapon = reinterpret_cast<c_base_player_weapon*>(entity);
            if (weapon->m_hOwnerEntity() == nullptr)
			{
                auto scene_node = weapon->m_pGameSceneNode();
				if (scene_node) {
				out_data = {};
                    out_data.is_dropped = true;
                    out_data.position = m_memory->read_t<vector_t>(scene_node + g_offsets::m_vecAbsOrigin);
				return true;
			}
		}
	}
}
    return false;
}

