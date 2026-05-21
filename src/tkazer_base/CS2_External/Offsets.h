#pragma once
#include <Windows.h>
#include <string>

// From: https://github.com/a2x/cs2-dumper/blob/main/generated/client.dll.hpp
namespace Offset
{
	inline DWORD EntityList;
	inline DWORD Matrix;
	inline DWORD ViewAngle;
	inline DWORD LocalPlayerController;
	inline DWORD LocalPlayerPawn;
	inline DWORD ForceJump;
	inline DWORD GlobalVars;

	// 1. Сначала объявляем ИМЯ структуры
	struct EntityOffsets
	{
		DWORD Health = 0;       // БЫЛО 0x32C -> СТАЛО 0x34C (m_iHealth)
		DWORD TeamID = 0;       // БЫЛО 0x3BF -> СТАЛО 0x3EB (m_iTeamNum)
		DWORD IsAlive = 0;      // БЫЛО 0x7DC -> СТАЛО 0x914 (m_bPawnIsAlive)
		DWORD PlayerPawn = 0;   // БЫЛО 0x5F4 -> СТАЛО 0x90C (m_hPlayerPawn)
		DWORD iszPlayerName = 0;// ОСТАЛОСЬ 0x6F4
	};
	// 2. Затем создаем глобальную inline переменную этого типа
	inline EntityOffsets Entity;

	// То же самое для Pawn
	struct PawnOffsets
	{
		DWORD Pos = 0;
		DWORD MaxHealth = 0;
		DWORD CurrentHealth = 0;
		DWORD GameSceneNode = 0;
		DWORD BoneArray = 0;
		DWORD angEyeAngles = 0;
		DWORD vecLastClipCameraPos = 0;
		DWORD pClippingWeapon = 0;
		DWORD iShotsFired = 0;
		DWORD flFlashDuration = 0;
		DWORD aimPunchAngle = 0;
		DWORD aimPunchCache = 0;
		DWORD iIDEntIndex = 0;
		DWORD iTeamNum = 0;
		DWORD CameraServices = 0;
		DWORD iFovStart = 0;
		DWORD fFlags = 0;
		DWORD bSpottedByMask = 0;
	};
	inline PawnOffsets Pawn;

	// И для GlobalVars (чтобы и они не отвалились в будущем)
	struct GlobalVarOffsets
	{
		DWORD RealTime = 0x00;
		DWORD FrameCount = 0x04;
		DWORD MaxClients = 0x10;
		DWORD IntervalPerTick = 0x14;
		DWORD CurrentTime = 0x2C;
		DWORD CurrentTime2 = 0x30;
		DWORD TickCount = 0x40;
		DWORD IntervalPerTick2 = 0x44;
		DWORD CurrentNetchan = 0x0048;
		DWORD CurrentMap = 0x0180;
		DWORD CurrentMapName = 0x0188;
	};
	inline GlobalVarOffsets GlobalVar;

	namespace Signatures
	{
		const std::string GlobalVars = "48 89 0D ?? ?? ?? ?? 48 89 41";
		const std::string ViewMatrix = "48 8D 0D ?? ?? ?? ?? 48 C1 E0 06";
		const std::string ViewAngles = "48 8B 0D ?? ?? ?? ?? E9 ?? ?? ?? ?? CC CC CC CC 40 55";
		const std::string EntityList = "48 8B 0D ?? ?? ?? ?? 48 89 7C 24 ?? 8B FA C1";
		const std::string LocalPlayerController = "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 4F";
		const std::string ForceJump = "48 8B 05 ?? ?? ?? ?? 48 8D 1D ?? ?? ?? ?? 48 89 45";
		const std::string LocalPlayerPawn = "48 8D 05 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC 48 83 EC ?? 8B 0D";
	}

	bool UpdateOffsets();
}