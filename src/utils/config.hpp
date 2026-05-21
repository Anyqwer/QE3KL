#pragma once

struct config_data_t
{
	bool m_use_localhost;
	std::string m_local_ip;
	std::string m_public_ip;
	
	// ESP Settings
	struct esp_settings_t
	{
		bool ShowBoxESP = true;
		bool ShowSkeleton = true;
		bool ShowHealthBar = true;
		bool ShowName = true;
		bool ShowWeapon = true;
		bool ShowTeamESP = false;
		uint32_t BoxColorEnemy = 0xFF0000FF;
		uint32_t BoxColorTeammate = 0x00FF00FF;
		uint32_t SkeletonColor = 0xFFFFFFFF;
		float BoxThickness = 1.0f;
		bool BoxFill = false;
		uint32_t BoxFillColor = 0x40000000;
		bool HealthBarVertical = true;
		float HealthBarWidth = 4.0f;
		bool ESPEnabled = true;
		bool AutoSwapEnabled = false;
		float ExtrapolationAmount = 30.0f;
		bool AntiOBS = false;
		bool BhopEnabled = false;
		bool KillSoundEnabled = false;
		float KillSoundVolume = 0.5f;
		bool AutoPistolEnabled = false;
		int AutoPistolKey = 2;  // 0=Mouse3, 1=Mouse4, 2=Mouse5, 3=ALT
	} esp_settings;

	// Triggerbot Settings
	struct triggerbot_settings_t
	{
		bool TriggerEnabled = false;
		int TriggerKey = 18;  // 18=Alt, 5=Mouse4, 6=Mouse5
		int TriggerDelay = 20; // Delay in milliseconds
		bool TriggerFriendlyFire = false;
	} triggerbot_settings;
};

namespace cfg
{
	bool setup(config_data_t& config_data);
	bool save(const config_data_t& config_data);
}