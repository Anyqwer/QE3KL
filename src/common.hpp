#pragma once

/* current build of cs2_webradar */
#define CS2_WEBRADAR_VERSION "v1.2.9"

/* game modules */
#define CLIENT_DLL MOD_NAME("client.dll")
#define ENGINE2_DLL MOD_NAME("engine2.dll")
#define SCHEMASYSTEM_DLL MOD_NAME("schemasystem.dll")

/* game signatures */
#define GET_SCHEMA_SYSTEM "48 89 05 ? ? ? ? 4c 8d 0d ? ? ? ? 33 c0"
#define GET_ENTITY_LIST "48 8b 0d ? ? ? ? 48 89 7c 24 ? 8b fa c1 eb"
#define GET_GLOBAL_VARS "48 89 15 ? ? ? ? 48 89 42"
#define GET_LOCAL_PLAYER_CONTROLLER "4c 8d 05 ? ? ? ? 33 d2 4d 8b 04 c0"

/* custom defines */
#define LOG_INFO(str, ...) \
    printf(" [info] " str "\n", __VA_ARGS__)

#define LOG_WARNING(str, ...) \
    printf(" [warning] " str "\n", __VA_ARGS__)

#define LOG_ERROR(str, ...) \
    { \
        const auto filename = std::filesystem::path(__FILE__).filename().string(); \
        printf(" [error] [%s:%d] " str "\n", filename.c_str(), __LINE__, __VA_ARGS__); \
        std::this_thread::sleep_for(std::chrono::seconds(5)); \
    }

/* global offsets namespace */
namespace g_offsets
{
    // client.dll offsets
    inline std::ptrdiff_t global_vars = 0;
    inline std::ptrdiff_t game_entity_system = 0;
    inline std::ptrdiff_t view_matrix = 0;
    inline std::ptrdiff_t view_render = 0;
    inline std::ptrdiff_t planted_c4 = 0;
    inline std::ptrdiff_t local_player_controller = 0;
    inline std::ptrdiff_t local_player_pawn = 0;
    inline std::ptrdiff_t force_jump = 0;
    inline std::ptrdiff_t view_angle = 0;
    inline std::ptrdiff_t m_iIDEntIndex = 0;
    inline std::ptrdiff_t m_pActionTrackingServices = 0;
    inline std::ptrdiff_t m_iNumRoundKills = 0;
    
    // SDK entity offsets
    inline std::ptrdiff_t m_pCollision = 0;
    inline std::ptrdiff_t m_pAimPunchServices = 0;
    inline std::ptrdiff_t m_iShotsFired = 0;
    inline std::ptrdiff_t m_fFlags = 0;

    // tkazer_base entity offsets
    inline std::ptrdiff_t m_iHealth = 0;
    inline std::ptrdiff_t m_iTeamNum = 0;
    inline std::ptrdiff_t m_lifeState = 0;
    inline std::ptrdiff_t m_vecOrigin = 0;
    inline std::ptrdiff_t m_angEyeAngles = 0;
    inline std::ptrdiff_t m_pGameSceneNode = 0;
    inline std::ptrdiff_t m_modelState = 0;
    inline std::ptrdiff_t m_bIsLocalPlayerController = 0;
    inline std::ptrdiff_t m_hPlayerPawn = 0;
    inline std::ptrdiff_t m_iszPlayerName = 0;
    inline std::ptrdiff_t m_pClippingWeapon = 0;
    inline std::ptrdiff_t m_pCameraServices = 0;
    inline std::ptrdiff_t m_iFOV = 0;
    inline std::ptrdiff_t m_bSpottedByMask = 0;
    inline std::ptrdiff_t m_flFlashDuration = 0;
    inline std::ptrdiff_t m_aimPunchAngle = 0;
    inline std::ptrdiff_t m_aimPunchCache = 0;
    
    // GameSceneNode offsets
    inline std::ptrdiff_t bone_array = 0;
    inline std::ptrdiff_t m_vecAbsOrigin = 0;

    // Inferno (molotov) offsets
    inline std::ptrdiff_t m_fireCount = 0;
    inline std::ptrdiff_t m_bFireIsBurning = 0;
    inline std::ptrdiff_t m_firePositions = 0;

    // Smoke grenade offsets
    inline std::ptrdiff_t m_bDidSmokeEffect = 0;
    inline std::ptrdiff_t m_nSmokeEffectTickBegin = 0;

    // schema system offsets
    inline std::ptrdiff_t schema_field_name = 0;
    inline std::ptrdiff_t schema_field_offset = 0;
    inline std::ptrdiff_t schema_binary_name = 0;
    inline std::ptrdiff_t schema_fields_count = 0;
    inline std::ptrdiff_t schema_fields_data = 0;
    inline std::ptrdiff_t type_scope_module_name = 0;
    inline std::ptrdiff_t type_scope_hash_classes = 0;
    inline std::ptrdiff_t schema_type_scopes_size = 0;
    inline std::ptrdiff_t schema_type_scopes_data = 0;
}