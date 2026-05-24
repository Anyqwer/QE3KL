#pragma once

enum class e_team : uint8_t
{
	none,
	spec,
	t,
	ct
};

enum class e_colors : uint32_t
{
	blue,
	green,
	yellow,
	orange,
	purple,
	white
};

enum class e_weapon_type : uint32_t
{
	knife,
	pistol,
	submachinegun,
	rifle,
	shotgun,
	sniper_rifle,
	machinegun,
	c4,
	taser,
	// grenade,  // DISABLED - not actively used
	equipment,
	stackableitem,
	fists,
	breachcharge,
	bumpmine,
	tablet,
	melee,
	shield,
	zone_repulsor,
	unknown
};

class c_entity_identity
{
public:
	SCHEMA_ADD_OFFSET(uintptr_t, m_pClassInfo, 0x08);
	SCHEMA_ADD_OFFSET(uint32_t, m_Idx, 0x10);
	SCHEMA_ADD_OFFSET(const char*, m_designerName, cs2_dumper::schemas::client_dll::CEntityIdentity::m_designerName);
	SCHEMA_ADD_OFFSET(uint32_t, m_flags, cs2_dumper::schemas::client_dll::CEntityIdentity::m_flags);

	bool is_valid()
	{
		return m_Idx() != INVALID_EHANDLE_IDX;
	}

	int32_t get_entry_idx()
	{
		if (!is_valid())
			return ENT_ENTRY_MASK;

		return m_Idx() & ENT_ENTRY_MASK;
	}

	int32_t get_serial_number()
	{
		return m_Idx() >> NUM_SERIAL_NUM_SHIFT_BITS;
	}
};

class c_entity_instance
{
public:
	SCHEMA_ADD_OFFSET(c_entity_identity*, m_pEntity, cs2_dumper::schemas::client_dll::CEntityInstance::m_pEntity);

	const c_base_handle get_ref_e_handle();
	const std::string get_schema_class_name();
};

class c_game_scene_node
{
public:
	SCHEMA_ADD_OFFSET(f_vector, m_vecAbsOrigin, cs2_dumper::schemas::client_dll::CGameSceneNode::m_vecAbsOrigin);
};

class c_collision_property
{
public:
	SCHEMA_ADD_OFFSET(f_vector, m_vecMins, cs2_dumper::schemas::client_dll::CCollisionProperty::m_vecMins);
	SCHEMA_ADD_OFFSET(f_vector, m_vecMaxs, cs2_dumper::schemas::client_dll::CCollisionProperty::m_vecMaxs);
};

class c_aim_punch_services
{
public:
	SCHEMA_ADD_OFFSET(f_vector, m_predictableBaseAngle, cs2_dumper::schemas::client_dll::CCSPlayer_AimPunchServices::m_predictableBaseAngle);
};

class c_base_entity : public c_entity_instance
{
public:
	SCHEMA_ADD_OFFSET(c_game_scene_node*, m_pGameSceneNode, cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode);
	SCHEMA_ADD_OFFSET(int32_t, m_iHealth, cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
	SCHEMA_ADD_OFFSET(e_team, m_iTeamNum, cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
	SCHEMA_ADD_OFFSET(c_base_entity*, m_hOwnerEntity, cs2_dumper::schemas::client_dll::C_BaseEntity::m_hOwnerEntity);
	SCHEMA_ADD_OFFSET(f_vector, m_vecAbsVelocity, cs2_dumper::schemas::client_dll::C_BaseEntity::m_vecAbsVelocity);
	SCHEMA_ADD_OFFSET(c_collision_property*, m_pCollision, g_offsets::m_pCollision);  // CCollisionProperty*

	const f_vector& get_scene_origin();
};

class c_player_weapon_services
{
public:
	SCHEMA_ADD_OFFSET(c_base_handle, m_hActiveWeapon, cs2_dumper::schemas::client_dll::CPlayer_WeaponServices::m_hActiveWeapon);
	SCHEMA_ADD_OFFSET(c_network_utl_vector_base<class c_base_player_weapon>, m_hMyWeapons, cs2_dumper::schemas::client_dll::CPlayer_WeaponServices::m_hMyWeapons);
};

class c_player_item_services
{
public:
	SCHEMA_ADD_OFFSET(bool, m_bHasDefuser, cs2_dumper::schemas::client_dll::CCSPlayer_ItemServices::m_bHasDefuser);
	SCHEMA_ADD_OFFSET(bool, m_bHasHelmet, cs2_dumper::schemas::client_dll::CCSPlayer_ItemServices::m_bHasHelmet);
};

class c_base_player_pawn : public c_base_entity
{
public:
	SCHEMA_ADD_OFFSET(c_player_weapon_services*, m_pWeaponServices, cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_pWeaponServices);
	SCHEMA_ADD_OFFSET(c_player_item_services*, m_pItemServices, cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_pItemServices);
};

class c_cs_player_pawn : public c_base_player_pawn
{
public:
	SCHEMA_ADD_OFFSET(int32_t, m_ArmorValue, cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_ArmorValue);
	SCHEMA_ADD_OFFSET(f_vector, m_angEyeAngles, cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_angEyeAngles);
	SCHEMA_ADD_OFFSET(c_aim_punch_services*, m_pAimPunchServices, g_offsets::m_pAimPunchServices);  // CCSPlayer_AimPunchServices*
	SCHEMA_ADD_OFFSET(int32_t, m_iShotsFired, g_offsets::m_iShotsFired);  // int32
	SCHEMA_ADD_OFFSET(bool, m_bIsScoped, cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_bIsScoped);

	const std::string get_model_name();
};

class c_base_player_controller : public c_base_entity
{
public:
	SCHEMA_ADD_OFFSET(c_base_handle, m_hPawn, cs2_dumper::schemas::client_dll::CBasePlayerController::m_hPawn);
	SCHEMA_ADD_OFFSET(uint64_t, m_steamID, cs2_dumper::schemas::client_dll::CBasePlayerController::m_steamID);
};

class c_in_game_money_services
{
public:
	SCHEMA_ADD_OFFSET(int32_t, m_iAccount, cs2_dumper::schemas::client_dll::CCSPlayerController_InGameMoneyServices::m_iAccount);
};

class c_cs_player_controller : public c_base_player_controller
{
public:
	SCHEMA_ADD_OFFSET(c_in_game_money_services*, m_pInGameMoneyServices, cs2_dumper::schemas::client_dll::CCSPlayerController::m_pInGameMoneyServices);

	SCHEMA_ADD_OFFSET(e_colors, m_iCompTeammateColor, cs2_dumper::schemas::client_dll::CCSPlayerController::m_iCompTeammateColor);
	SCHEMA_ADD_STRING_OFFSET(m_sSanitizedPlayerName, cs2_dumper::schemas::client_dll::CCSPlayerController::m_sSanitizedPlayerName);

	static c_cs_player_controller* get_local_player_controller();
	c_cs_player_pawn* get_player_pawn();
	const e_colors get_color();
	const f_vector& get_vec_origin();
};

class c_planted_c4 : public c_base_entity
{
public:
	SCHEMA_ADD_OFFSET(bool, m_bBombTicking, cs2_dumper::schemas::client_dll::C_PlantedC4::m_bBombTicking);
	SCHEMA_ADD_OFFSET(int32_t, m_nBombSite, cs2_dumper::schemas::client_dll::C_PlantedC4::m_nBombSite);
	SCHEMA_ADD_OFFSET(float, m_flC4Blow, cs2_dumper::schemas::client_dll::C_PlantedC4::m_flC4Blow);
	SCHEMA_ADD_OFFSET(bool, m_bBombDefused, cs2_dumper::schemas::client_dll::C_PlantedC4::m_bBombDefused);
	SCHEMA_ADD_OFFSET(bool, m_bBeingDefused, cs2_dumper::schemas::client_dll::C_PlantedC4::m_bBeingDefused);
	SCHEMA_ADD_OFFSET(float, m_flDefuseCountDown, cs2_dumper::schemas::client_dll::C_PlantedC4::m_flDefuseCountDown);
};

class c_cs_weapon_base_v_data
{
public:
	SCHEMA_ADD_OFFSET(e_weapon_type, m_WeaponType, cs2_dumper::schemas::client_dll::CCSWeaponBaseVData::m_WeaponType);
	SCHEMA_ADD_STRING_OFFSET(m_szName, cs2_dumper::schemas::client_dll::CCSWeaponBaseVData::m_szName);
};

class c_base_player_weapon : public c_base_entity
{
public:
	SCHEMA_ADD_OFFSET(c_cs_weapon_base_v_data*, m_WeaponData, cs2_dumper::schemas::client_dll::C_BaseEntity::m_nSubclassID + 0x08);
	SCHEMA_ADD_OFFSET(bool, m_bInReload, cs2_dumper::schemas::client_dll::C_CSWeaponBase::m_bInReload);

	c_base_player_weapon* get(const int32_t idx);
};

// GRENADE ENTITY CLASSES - DISABLED (not actively used)
/*
class c_base_cs_grenade_projectile : public c_base_entity
{
public:
	SCHEMA_ADD_OFFSET(f_vector, m_vInitialPosition, cs2_dumper::schemas::client_dll::C_BaseCSGrenadeProjectile::m_vInitialPosition);
	SCHEMA_ADD_OFFSET(f_vector, m_vInitialVelocity, cs2_dumper::schemas::client_dll::C_BaseCSGrenadeProjectile::m_vInitialVelocity);
	SCHEMA_ADD_OFFSET(int32_t, m_nBounces, cs2_dumper::schemas::client_dll::C_BaseCSGrenadeProjectile::m_nBounces);
	SCHEMA_ADD_OFFSET(float, m_flSpawnTime, cs2_dumper::schemas::client_dll::C_BaseCSGrenadeProjectile::m_flSpawnTime);
	SCHEMA_ADD_OFFSET(bool, m_bExplodeEffectBegan, cs2_dumper::schemas::client_dll::C_BaseCSGrenadeProjectile::m_bExplodeEffectBegan);
};

class c_smoke_grenade_projectile : public c_base_entity
{
public:
	SCHEMA_ADD_OFFSET(bool, m_bDidSmokeEffect, cs2_dumper::schemas::client_dll::C_SmokeGrenadeProjectile::m_bDidSmokeEffect);
	SCHEMA_ADD_OFFSET(f_vector, m_vSmokeDetonationPos, cs2_dumper::schemas::client_dll::C_SmokeGrenadeProjectile::m_vSmokeDetonationPos);
};

class c_inferno : public c_base_entity
{
public:
	SCHEMA_ADD_OFFSET(bool, m_bFireIsBurning, cs2_dumper::schemas::client_dll::C_Inferno::m_bFireIsBurning);
};
*/