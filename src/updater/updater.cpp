#include "pch.hpp"
#include "updater.hpp"
#include "common.hpp"
#include <fstream>
#include <thread>
#include "../tkazer_base/CS2_External/Offsets.h"
using json = nlohmann::json;

namespace updater
{
    void run_dumper()
    {
        LOG_INFO("running cs2-dumper.exe to generate fresh offsets...");

        // Запускаем cs2-dumper.exe
        system("cs2-dumper.exe");

        // Ждем сохранения файлов
        std::this_thread::sleep_for(std::chrono::seconds(2));

        LOG_INFO("cs2-dumper.exe execution completed");
    }

    bool load_json_offsets()
    {
        try
        {
            // Загружаем offsets.json
            std::ifstream offsets_file("output/offsets.json");
            if (!offsets_file.is_open())
            {
                LOG_ERROR("failed to open offsets.json");
                return false;
            }

            json offsets_json;
            offsets_file >> offsets_json;
            offsets_file.close();

            // Проверяем наличие client.dll
            if (!offsets_json.contains("client.dll"))
            {
                LOG_ERROR("offsets.json does not contain 'client.dll' section");
                return false;
            }

            auto client_dll = offsets_json["client.dll"];

            // Загружаем client.dll оффсеты
            if (client_dll.contains("dwGlobalVars"))
                g_offsets::global_vars = client_dll["dwGlobalVars"].get<std::ptrdiff_t>();
            else
                LOG_ERROR("offsets.json missing dwGlobalVars");

            if (client_dll.contains("dwGameEntitySystem"))
                g_offsets::game_entity_system = client_dll["dwGameEntitySystem"].get<std::ptrdiff_t>();
            else
                LOG_ERROR("offsets.json missing dwGameEntitySystem");

            if (client_dll.contains("dwPlantedC4"))
                g_offsets::planted_c4 = client_dll["dwPlantedC4"].get<std::ptrdiff_t>();
            else
                LOG_ERROR("offsets.json missing dwPlantedC4");

            if (client_dll.contains("dwViewMatrix"))
                g_offsets::view_matrix = client_dll["dwViewMatrix"].get<std::ptrdiff_t>();
            else
                LOG_ERROR("offsets.json missing dwViewMatrix");

            if (client_dll.contains("dwViewRender"))
                g_offsets::view_render = client_dll["dwViewRender"].get<std::ptrdiff_t>();
            else
                LOG_ERROR("offsets.json missing dwViewRender");

            if (client_dll.contains("dwLocalPlayerController"))
                g_offsets::local_player_controller = client_dll["dwLocalPlayerController"].get<std::ptrdiff_t>();
            else
                LOG_ERROR("offsets.json missing dwLocalPlayerController");

            LOG_INFO("client.dll offsets loaded successfully");

            // Load schema offsets from client_dll.json
            std::ifstream client_dll_json_file("output/client_dll.json");
            if (!client_dll_json_file.is_open())
            {
                LOG_ERROR("failed to open client_dll.json");
                return false;
            }

            json client_dll_json;
            client_dll_json_file >> client_dll_json;
            client_dll_json_file.close();

            if (client_dll_json.contains("client.dll") && 
                client_dll_json["client.dll"].contains("classes") &&
                client_dll_json["client.dll"]["classes"].contains("C_CSPlayerPawn") &&
                client_dll_json["client.dll"]["classes"]["C_CSPlayerPawn"].contains("fields") &&
                client_dll_json["client.dll"]["classes"]["C_CSPlayerPawn"]["fields"].contains("m_iIDEntIndex"))
            {
                g_offsets::m_iIDEntIndex = client_dll_json["client.dll"]["classes"]["C_CSPlayerPawn"]["fields"]["m_iIDEntIndex"].get<std::ptrdiff_t>();
                LOG_INFO("m_iIDEntIndex loaded from client_dll.json: %td", g_offsets::m_iIDEntIndex);
            }
            else
            {
                LOG_ERROR("client_dll.json missing m_iIDEntIndex in C_CSPlayerPawn fields");
            }

            if (client_dll_json["client.dll"].contains("classes") &&
                client_dll_json["client.dll"]["classes"].contains("CCSPlayerPawnBase") &&
                client_dll_json["client.dll"]["classes"]["CCSPlayerPawnBase"].contains("fields")) {
                
                const auto& fields = client_dll_json["client.dll"]["classes"]["CCSPlayerPawnBase"]["fields"];
                if (fields.contains("m_iShotsFired")) {
                    g_offsets::m_iShotsFired = std::stoull(fields["m_iShotsFired"]["offset"].get<std::string>(), nullptr, 16);
                }
                
                // m_fFlags is in C_BaseEntity, not CCSPlayerPawnBase - will be loaded below
            }

            if (client_dll_json.contains("client.dll") && 
                client_dll_json["client.dll"].contains("classes"))
            {
                auto& classes = client_dll_json["client.dll"]["classes"];

            // Helper: safely read an offset value which may be a hex string ("0x1234") or a number
            auto read_offset = [](const nlohmann::json& node) -> std::ptrdiff_t {
                try {
                    if (node.is_string()) {
                        const auto s = node.get<std::string>();
                        // strip possible 0x prefix
                        return static_cast<std::ptrdiff_t>(std::stoull(s, nullptr, 16));
                    }
                    else if (node.is_number_unsigned()) {
                        return static_cast<std::ptrdiff_t>(node.get<std::uint64_t>());
                    }
                    else if (node.is_number_integer()) {
                        return static_cast<std::ptrdiff_t>(node.get<std::int64_t>());
                    }
                } catch (...) {
                }
                return 0;
            };
                
                // C_BaseEntity::m_pCollision
                if (classes.contains("C_BaseEntity") && 
                    classes["C_BaseEntity"].contains("fields") &&
                    classes["C_BaseEntity"]["fields"].contains("m_pCollision"))
                {
                    g_offsets::m_pCollision = classes["C_BaseEntity"]["fields"]["m_pCollision"].get<std::ptrdiff_t>();
                }
                else
                {
                    LOG_ERROR("client_dll.json missing m_pCollision in C_BaseEntity fields");
                }

                // C_BaseEntity::m_hOwnerEntity
                if (classes.contains("C_BaseEntity") && 
                    classes["C_BaseEntity"].contains("fields") &&
                    classes["C_BaseEntity"]["fields"].contains("m_hOwnerEntity"))
                {
                    g_offsets::m_hOwnerEntity = classes["C_BaseEntity"]["fields"]["m_hOwnerEntity"].get<std::ptrdiff_t>();
                }
                else
                {
                    LOG_ERROR("client_dll.json missing m_hOwnerEntity in C_BaseEntity fields");
                }
                
                // C_BaseEntity::m_fFlags
                if (classes.contains("C_BaseEntity") &&
                    classes["C_BaseEntity"].contains("fields") &&
                    classes["C_BaseEntity"]["fields"].contains("m_fFlags"))
                {
                    g_offsets::m_fFlags = classes["C_BaseEntity"]["fields"]["m_fFlags"].get<std::ptrdiff_t>();
                }
                else
                {
                    LOG_ERROR("client_dll.json missing m_fFlags in C_BaseEntity fields");
                }
                
                // C_CSPlayerPawn::m_pAimPunchServices
                if (classes.contains("C_CSPlayerPawn") && 
                    classes["C_CSPlayerPawn"].contains("fields") &&
                    classes["C_CSPlayerPawn"]["fields"].contains("m_pAimPunchServices"))
                {
                    g_offsets::m_pAimPunchServices = classes["C_CSPlayerPawn"]["fields"]["m_pAimPunchServices"].get<std::ptrdiff_t>();
                }
                else
                {
                    LOG_ERROR("client_dll.json missing m_pAimPunchServices in C_CSPlayerPawn fields");
                }
                
                // C_CSPlayerPawn::m_iShotsFired
                if (classes.contains("C_CSPlayerPawn") && 
                    classes["C_CSPlayerPawn"].contains("fields") &&
                    classes["C_CSPlayerPawn"]["fields"].contains("m_iShotsFired"))
                {
                    g_offsets::m_iShotsFired = classes["C_CSPlayerPawn"]["fields"]["m_iShotsFired"].get<std::ptrdiff_t>();
                }
                else
                {
                    LOG_ERROR("client_dll.json missing m_iShotsFired in C_CSPlayerPawn fields");
                }

                // Load ActionTrackingServices offset
                if (client_dll_json["client.dll"].contains("classes") &&
                    client_dll_json["client.dll"].contains("CCSPlayerController") &&
                    client_dll_json["client.dll"]["classes"]["CCSPlayerController"].contains("fields") &&
                    client_dll_json["client.dll"]["classes"]["CCSPlayerController"]["fields"].contains("m_pActionTrackingServices"))
                {
                    g_offsets::m_pActionTrackingServices = client_dll_json["client.dll"]["classes"]["CCSPlayerController"]["fields"]["m_pActionTrackingServices"].get<std::ptrdiff_t>();
                }
                else
                {
                    LOG_ERROR("client_dll.json missing m_pActionTrackingServices in CCSPlayerController fields");
                }
                // =========================================================================
                // === МОСТ: ДИНАМИЧЕСКОЕ ОБНОВЛЕНИЕ ХАРДКОДНЫХ СТРУКТУР (tkazer_base) ===
                // =========================================================================

                // 1. CCSPlayerController (Имена и состояния)
                if (classes.contains("CCSPlayerController") && classes["CCSPlayerController"].contains("fields")) {
                    auto& f = classes["CCSPlayerController"]["fields"];
                    if (f.contains("m_bPawnIsAlive")) Offset::Entity.IsAlive = f["m_bPawnIsAlive"].get<std::ptrdiff_t>();
                    if (f.contains("m_hPlayerPawn")) Offset::Entity.PlayerPawn = f["m_hPlayerPawn"].get<std::ptrdiff_t>();
                    if (f.contains("m_iszPlayerName")) Offset::Entity.iszPlayerName = f["m_iszPlayerName"].get<std::ptrdiff_t>();
                }

                // 2. C_BaseEntity (Здоровье, Команда, Кости, Флаги)
                if (classes.contains("C_BaseEntity") && classes["C_BaseEntity"].contains("fields")) {
                    auto& f = classes["C_BaseEntity"]["fields"];
                    if (f.contains("m_iHealth")) {
                        Offset::Entity.Health = f["m_iHealth"].get<std::ptrdiff_t>();
                        g_offsets::m_iHealth = f["m_iHealth"].get<std::ptrdiff_t>();
                        Offset::Pawn.CurrentHealth = f["m_iHealth"].get<std::ptrdiff_t>();
                    }
                    if (f.contains("m_iMaxHealth")) Offset::Pawn.MaxHealth = f["m_iMaxHealth"].get<std::ptrdiff_t>();
                    if (f.contains("m_iTeamNum")) {
                        Offset::Entity.TeamID = f["m_iTeamNum"].get<std::ptrdiff_t>();
                        g_offsets::m_iTeamNum = f["m_iTeamNum"].get<std::ptrdiff_t>();
                        Offset::Pawn.iTeamNum = f["m_iTeamNum"].get<std::ptrdiff_t>();
                    }
                    if (f.contains("m_pGameSceneNode")) {
                        Offset::Pawn.GameSceneNode = f["m_pGameSceneNode"].get<std::ptrdiff_t>();
                        g_offsets::m_pGameSceneNode = f["m_pGameSceneNode"].get<std::ptrdiff_t>();
                    }
                    if (f.contains("m_fFlags")) Offset::Pawn.fFlags = f["m_fFlags"].get<std::ptrdiff_t>();
                }

                // 3. C_CSPlayerPawn (Триггер, Взгляд, Спот)
                if (classes.contains("C_CSPlayerPawn") && classes["C_CSPlayerPawn"].contains("fields")) {
                    auto& f = classes["C_CSPlayerPawn"]["fields"];
                    if (f.contains("m_angEyeAngles")) Offset::Pawn.angEyeAngles = f["m_angEyeAngles"].get<std::ptrdiff_t>();
                    if (f.contains("m_iShotsFired")) Offset::Pawn.iShotsFired = f["m_iShotsFired"].get<std::ptrdiff_t>();
                    if (f.contains("m_pAimPunchServices")) Offset::Pawn.aimPunchAngle = f["m_pAimPunchServices"].get<std::ptrdiff_t>();
                    if (f.contains("m_iIDEntIndex")) Offset::Pawn.iIDEntIndex = f["m_iIDEntIndex"].get<std::ptrdiff_t>();
                    if (f.contains("m_bSpottedByMask")) Offset::Pawn.bSpottedByMask = f["m_bSpottedByMask"].get<std::ptrdiff_t>();
                }

                // 4. C_BasePlayerPawn (Координаты, Камеры, Оружие)
                if (classes.contains("C_BasePlayerPawn") && classes["C_BasePlayerPawn"].contains("fields")) {
                    auto& f = classes["C_BasePlayerPawn"]["fields"];
                    if (f.contains("m_vOldOrigin")) Offset::Pawn.Pos = f["m_vOldOrigin"].get<std::ptrdiff_t>();
                    if (f.contains("m_vecLastCameraSetupLocalOrigin")) Offset::Pawn.vecLastClipCameraPos = f["m_vecLastCameraSetupLocalOrigin"].get<std::ptrdiff_t>();
                    if (f.contains("m_pWeaponServices")) Offset::Pawn.pClippingWeapon = f["m_pWeaponServices"].get<std::ptrdiff_t>();
                    if (f.contains("m_pCameraServices")) Offset::Pawn.CameraServices = f["m_pCameraServices"].get<std::ptrdiff_t>();
                }

                // 5. C_CSPlayerPawnBase (Флешки)
                if (classes.contains("C_CSPlayerPawnBase") && classes["C_CSPlayerPawnBase"].contains("fields")) {
                    auto& f = classes["C_CSPlayerPawnBase"]["fields"];
                    if (f.contains("m_flFlashDuration")) Offset::Pawn.flFlashDuration = f["m_flFlashDuration"].get<std::ptrdiff_t>();
                }

                // 6. CCSPlayerBase_CameraServices (FOV)
                if (classes.contains("CCSPlayerBase_CameraServices") && classes["CCSPlayerBase_CameraServices"].contains("fields")) {
                    auto& f = classes["CCSPlayerBase_CameraServices"]["fields"];
                    if (f.contains("m_iFOVStart")) Offset::Pawn.iFovStart = f["m_iFOVStart"].get<std::ptrdiff_t>();
                }
                
                LOG_INFO("tkazer_base struct offsets dynamically overwritten from JSON!");
                // =========================================================================
                // Load NumRoundKills offset
                if (client_dll_json["client.dll"].contains("classes") &&
                    client_dll_json["client.dll"].contains("CCSPlayerController_ActionTrackingServices") &&
                    client_dll_json["client.dll"]["classes"]["CCSPlayerController_ActionTrackingServices"].contains("fields") &&
                    client_dll_json["client.dll"]["classes"]["CCSPlayerController_ActionTrackingServices"]["fields"].contains("m_iNumRoundKills"))
                {
                    g_offsets::m_iNumRoundKills = client_dll_json["client.dll"]["classes"]["CCSPlayerController_ActionTrackingServices"]["fields"]["m_iNumRoundKills"].get<std::ptrdiff_t>();
                    LOG_INFO("m_iNumRoundKills loaded from client_dll.json: %td", g_offsets::m_iNumRoundKills);
                }
                else
                {
                    LOG_ERROR("client_dll.json missing m_iNumRoundKills in CCSPlayerController_ActionTrackingServices fields");
                }
            }
            else
            {
                LOG_ERROR("client_dll.json missing classes section");
            }

            // Schema system offsets - статические значения (обычно не меняются)
            g_offsets::schema_field_name = 0x00;
            g_offsets::schema_field_offset = 0x10;
            g_offsets::schema_binary_name = 0x08;
            g_offsets::schema_fields_count = 0x1c;
            g_offsets::schema_fields_data = 0x28;
            g_offsets::type_scope_module_name = 0x08;
            g_offsets::type_scope_hash_classes = 0x540;
            g_offsets::schema_type_scopes_size = 0x190;
            g_offsets::schema_type_scopes_data = 0x198;

            LOG_INFO("schema system offsets set (static values)");
            LOG_INFO("all offsets loaded successfully");
            return true;
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("exception while loading offsets: %s", e.what());
            return false;
        }
    }

    bool load_dynamic_offsets()
    {
        try {
            // Reuse existing load_json_offsets for basic offsets
            if (!load_json_offsets()) return false;

            // Load client_dll.json and check for inferno / smoke fields
            std::ifstream client_dll_json_file("output/client_dll.json");
            if (!client_dll_json_file.is_open()) {
                LOG_ERROR("failed to open client_dll.json for dynamic offsets");
                return false;
            }

            nlohmann::json client_dll_json;
            client_dll_json_file >> client_dll_json;
            client_dll_json_file.close();

            if (!client_dll_json.contains("client.dll") || !client_dll_json["client.dll"].contains("classes"))
                return false;

            auto& classes = client_dll_json["client.dll"]["classes"];

            // Helper: safely read an offset value which may be a hex string ("0x1234") or a number
            auto read_offset = [](const nlohmann::json& node) -> std::ptrdiff_t {
                try {
                    if (node.is_string()) {
                        const auto s = node.get<std::string>();
                        return static_cast<std::ptrdiff_t>(std::stoull(s, nullptr, 16));
                    }
                    else if (node.is_number_unsigned()) {
                        return static_cast<std::ptrdiff_t>(node.get<std::uint64_t>());
                    }
                    else if (node.is_number_integer()) {
                        return static_cast<std::ptrdiff_t>(node.get<std::int64_t>());
                    }
                    else if (node.is_object() && node.contains("offset")) {
                        const auto off = node["offset"];
                        if (off.is_string()) return static_cast<std::ptrdiff_t>(std::stoull(off.get<std::string>(), nullptr, 16));
                        if (off.is_number_unsigned()) return static_cast<std::ptrdiff_t>(off.get<std::uint64_t>());
                        if (off.is_number_integer()) return static_cast<std::ptrdiff_t>(off.get<std::int64_t>());
                    }
                } catch (...) {
                }
                return 0;
            };

            // C_Inferno
            if (classes.contains("C_Inferno") && classes["C_Inferno"].contains("fields")) {
                auto& f = classes["C_Inferno"]["fields"];
                if (f.contains("m_fireCount")) {
                    if (f["m_fireCount"].is_object() && f["m_fireCount"].contains("offset")) g_offsets::m_fireCount = read_offset(f["m_fireCount"]["offset"]);
                    else g_offsets::m_fireCount = read_offset(f["m_fireCount"]);
                }
                if (f.contains("m_bFireIsBurning")) {
                    if (f["m_bFireIsBurning"].is_object() && f["m_bFireIsBurning"].contains("offset")) g_offsets::m_bFireIsBurning = read_offset(f["m_bFireIsBurning"]["offset"]);
                    else g_offsets::m_bFireIsBurning = read_offset(f["m_bFireIsBurning"]);
                }
                if (f.contains("m_firePositions")) {
                    if (f["m_firePositions"].is_object() && f["m_firePositions"].contains("offset")) g_offsets::m_firePositions = read_offset(f["m_firePositions"]["offset"]);
                    else g_offsets::m_firePositions = read_offset(f["m_firePositions"]);
                }
            }

            // C_SmokeGrenadeProjectile
            if (classes.contains("C_SmokeGrenadeProjectile") && classes["C_SmokeGrenadeProjectile"].contains("fields")) {
                auto& f = classes["C_SmokeGrenadeProjectile"]["fields"];
                if (f.contains("m_bDidSmokeEffect")) {
                    if (f["m_bDidSmokeEffect"].is_object() && f["m_bDidSmokeEffect"].contains("offset")) g_offsets::m_bDidSmokeEffect = read_offset(f["m_bDidSmokeEffect"]["offset"]);
                    else g_offsets::m_bDidSmokeEffect = read_offset(f["m_bDidSmokeEffect"]);
                }
                if (f.contains("m_nSmokeEffectTickBegin")) {
                    if (f["m_nSmokeEffectTickBegin"].is_object() && f["m_nSmokeEffectTickBegin"].contains("offset")) g_offsets::m_nSmokeEffectTickBegin = read_offset(f["m_nSmokeEffectTickBegin"]["offset"]);
                    else g_offsets::m_nSmokeEffectTickBegin = read_offset(f["m_nSmokeEffectTickBegin"]);
                }
            }

            // Ensure GameSceneNode origin offset available
            if (classes.contains("CGameSceneNode") && classes["CGameSceneNode"].contains("fields") && classes["CGameSceneNode"]["fields"].contains("m_vecAbsOrigin")) {
                auto& node = classes["CGameSceneNode"]["fields"]["m_vecAbsOrigin"];
                if (node.is_object() && node.contains("offset")) g_offsets::m_vecAbsOrigin = read_offset(node["offset"]);
                else g_offsets::m_vecAbsOrigin = read_offset(node);
            }

            LOG_INFO("dynamic offsets loaded from client_dll.json");
            return true;
        }
        catch (const std::exception& e) {
            LOG_ERROR("exception while loading dynamic offsets: %s", e.what());
            return false;
        }
    }

} // namespace updater


