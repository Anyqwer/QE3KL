#include "pch.hpp"
#include "../utils/skCrypter.h"

const c_base_handle c_entity_instance::get_ref_e_handle()
{
	const auto entity = m_pEntity();
	if (!entity)
		return c_base_handle();

	return c_base_handle(entity->get_entry_idx(), entity->get_serial_number() - (entity->m_flags() & 1));
}

const std::string c_entity_instance::get_schema_class_name()
{
	const auto entity = m_pEntity();
	if (!entity)
		return {};

	const auto class_info = entity->m_pClassInfo();
	if (!class_info)
		return {};

	const auto unk1 = m_memory->read_t<uintptr_t>(class_info + 0x78);
	if (!unk1)
		return {};

	const auto unk2 = m_memory->read_t<uintptr_t>(unk1 + 0x08);
	if (!unk2)
		return {};

	auto name = m_memory->read_t<std::string>(unk2);
	if (name.empty())
		return {};

	return name;
}

const std::string c_cs_player_pawn::get_model_name()
{
	const auto model_name = m_memory->read_t<uintptr_t>(m_pGameSceneNode() + cs2_dumper::schemas::client_dll::CSkeletonInstance::m_modelState + cs2_dumper::schemas::client_dll::CModelState::m_ModelName);
	if (!model_name)
		return {};

	const auto model_path = m_memory->read_t<std::string>(model_name);
	if (model_path.empty())
		return {};

	return model_path.substr(model_path.rfind("/") + 1, model_path.rfind(".") - model_path.rfind("/") - 1);
}

c_cs_player_controller* c_cs_player_controller::get_local_player_controller()
{
	// Cache client_base to avoid repeated get_module_info calls
	static uintptr_t client_base_cached = 0;
	if (client_base_cached == 0)
	{
		auto module_info = m_memory->get_module_info(CLIENT_DLL);
		if (module_info.first.has_value())
			client_base_cached = module_info.first.value();
	}
	if (!client_base_cached)
		return {};

	return m_memory->read_t<c_cs_player_controller*>(client_base_cached + cs2_dumper::offsets::client_dll::dwLocalPlayerController);
}

c_cs_player_pawn* c_cs_player_controller::get_player_pawn()
{
	const auto& handle = m_hPawn();
	return i::m_game_entity_system->get<c_cs_player_pawn*>(handle);
}

const e_colors c_cs_player_controller::get_color()
{
	const auto color = m_iCompTeammateColor();
	if (color == static_cast<e_colors>(-1))
		return e_colors::white;

	return color;
}

const f_vector& c_cs_player_controller::get_vec_origin()
{
	const auto pawn = get_player_pawn();
	if (!pawn)
		return {};

	return pawn->get_scene_origin();
}

const f_vector& c_base_entity::get_scene_origin()
{
	const auto game_scene_node = m_pGameSceneNode();
	if (!game_scene_node)
		return {};

	return game_scene_node->m_vecAbsOrigin();
}

c_base_player_weapon* c_base_player_weapon::get(const int32_t idx)
{
	const auto handle = m_memory->read_t<int32_t>(this + idx * 0x4);
	if (handle == -1)
		return nullptr;

	return i::m_game_entity_system->get<c_base_player_weapon*>(handle & ENT_ENTRY_MASK);
}