#include "pch.hpp"

bool cfg::setup(config_data_t& config_data)
{
	std::ifstream file("config.json");
	if (!file.is_open())
	{
		LOG_WARNING("cannot open file 'config.json'");

		std::ofstream example_config("config.json");
		example_config << std::format("{}", R"({
    "m_use_localhost": true,
    "m_local_ip": "192.168.x.x",
    "m_public_ip": "x.x.x.x",
    "esp_settings": {
        "ShowBoxESP": true,
        "ShowSkeleton": true,
        "ShowHealthBar": true,
        "ShowName": true,
        "ShowWeapon": true,
        "BoxColorEnemy": 4278190335,
        "BoxColorTeammate": 4278255360,
        "SkeletonColor": 4294967295,
        "BoxThickness": 1.0,
        "BoxFill": false,
        "BoxFillColor": 1073741824,
        "HealthBarVertical": true,
        "HealthBarWidth": 4.0,
        "ESPEnabled": true
    }
})");

		return {};
	}

	const auto parsed_data = nlohmann::json::parse(file);
	if (parsed_data.empty())
	{
		LOG_ERROR("failed to parse 'config.json'");
		return {};
	}

	try
	{
		// Manual deserialization to avoid NLOHMANN issues
		config_data.m_use_localhost = parsed_data.value("m_use_localhost", true);
		config_data.m_local_ip = parsed_data.value("m_local_ip", "192.168.x.x");
		config_data.m_public_ip = parsed_data.value("m_public_ip", "x.x.x.x");
		
		if (parsed_data.contains("esp_settings"))
		{
			auto esp = parsed_data["esp_settings"];
			config_data.esp_settings.ShowBoxESP = esp.value("ShowBoxESP", true);
			config_data.esp_settings.ShowSkeleton = esp.value("ShowSkeleton", true);
			config_data.esp_settings.ShowHealthBar = esp.value("ShowHealthBar", true);
			config_data.esp_settings.ShowName = esp.value("ShowName", true);
			config_data.esp_settings.ShowWeapon = esp.value("ShowWeapon", true);
			config_data.esp_settings.ShowTeamESP = esp.value("ShowTeamESP", false);
			config_data.esp_settings.BoxColorEnemy = esp.value("BoxColorEnemy", 0xFF0000FF);
			config_data.esp_settings.BoxColorTeammate = esp.value("BoxColorTeammate", 0x00FF00FF);
			config_data.esp_settings.SkeletonColor = esp.value("SkeletonColor", 0xFFFFFFFF);
			config_data.esp_settings.BoxThickness = esp.value("BoxThickness", 1.0f);
			config_data.esp_settings.BoxFill = esp.value("BoxFill", false);
			config_data.esp_settings.BoxFillColor = esp.value("BoxFillColor", 0x40000000);
			config_data.esp_settings.HealthBarVertical = esp.value("HealthBarVertical", true);
			config_data.esp_settings.HealthBarWidth = esp.value("HealthBarWidth", 4.0f);
			config_data.esp_settings.ESPEnabled = esp.value("ESPEnabled", true);
			config_data.esp_settings.AutoSwapEnabled = esp.value("AutoSwapEnabled", false);
			config_data.esp_settings.ExtrapolationAmount = esp.value("ExtrapolationAmount", 30.0f);
			config_data.esp_settings.AntiOBS = esp.value("AntiOBS", false);
			config_data.esp_settings.BhopEnabled = esp.value("BhopEnabled", false);
			config_data.esp_settings.KillSoundEnabled = esp.value("KillSoundEnabled", false);
			config_data.esp_settings.KillSoundVolume = esp.value("KillSoundVolume", 0.5f);
			config_data.esp_settings.AutoPistolEnabled = esp.value("AutoPistolEnabled", false);
			config_data.esp_settings.AutoPistolKey = esp.value("AutoPistolKey", 2);
		}
		
		if (parsed_data.contains("triggerbot_settings"))
		{
			auto trigger = parsed_data["triggerbot_settings"];
			config_data.triggerbot_settings.TriggerEnabled = trigger.value("TriggerEnabled", false);
			config_data.triggerbot_settings.TriggerKey = trigger.value("TriggerKey", 18);
			config_data.triggerbot_settings.TriggerDelay = trigger.value("TriggerDelay", 20);
			config_data.triggerbot_settings.TriggerFriendlyFire = trigger.value("TriggerFriendlyFire", false);
		}
	}
	catch (const std::exception& e)
	{
		LOG_ERROR("failed to deserialize 'config_data_t' (%s)", e.what());
		return {};
	}

	return true;
}

bool cfg::save(const config_data_t& config_data)
{
	try
	{
		nlohmann::json j;
		j["m_use_localhost"] = config_data.m_use_localhost;
		j["m_local_ip"] = config_data.m_local_ip;
		j["m_public_ip"] = config_data.m_public_ip;
		
		nlohmann::json esp;
		esp["ShowBoxESP"] = config_data.esp_settings.ShowBoxESP;
		esp["ShowSkeleton"] = config_data.esp_settings.ShowSkeleton;
		esp["ShowHealthBar"] = config_data.esp_settings.ShowHealthBar;
		esp["ShowName"] = config_data.esp_settings.ShowName;
		esp["ShowWeapon"] = config_data.esp_settings.ShowWeapon;
		esp["ShowTeamESP"] = config_data.esp_settings.ShowTeamESP;
		esp["BoxColorEnemy"] = config_data.esp_settings.BoxColorEnemy;
		esp["BoxColorTeammate"] = config_data.esp_settings.BoxColorTeammate;
		esp["SkeletonColor"] = config_data.esp_settings.SkeletonColor;
		esp["BoxThickness"] = config_data.esp_settings.BoxThickness;
		esp["BoxFill"] = config_data.esp_settings.BoxFill;
		esp["BoxFillColor"] = config_data.esp_settings.BoxFillColor;
		esp["HealthBarVertical"] = config_data.esp_settings.HealthBarVertical;
		esp["HealthBarWidth"] = config_data.esp_settings.HealthBarWidth;
		esp["ESPEnabled"] = config_data.esp_settings.ESPEnabled;
		esp["AutoSwapEnabled"] = config_data.esp_settings.AutoSwapEnabled;
		esp["ExtrapolationAmount"] = config_data.esp_settings.ExtrapolationAmount;
		esp["AntiOBS"] = config_data.esp_settings.AntiOBS;
		esp["BhopEnabled"] = config_data.esp_settings.BhopEnabled;
		esp["KillSoundEnabled"] = config_data.esp_settings.KillSoundEnabled;
		esp["KillSoundVolume"] = config_data.esp_settings.KillSoundVolume;
		esp["AutoPistolEnabled"] = config_data.esp_settings.AutoPistolEnabled;
		esp["AutoPistolKey"] = config_data.esp_settings.AutoPistolKey;
		j["esp_settings"] = esp;
		
		nlohmann::json trigger;
		trigger["TriggerEnabled"] = config_data.triggerbot_settings.TriggerEnabled;
		trigger["TriggerKey"] = config_data.triggerbot_settings.TriggerKey;
		trigger["TriggerDelay"] = config_data.triggerbot_settings.TriggerDelay;
		trigger["TriggerFriendlyFire"] = config_data.triggerbot_settings.TriggerFriendlyFire;
		j["triggerbot_settings"] = trigger;
		
		std::ofstream file("config.json");
		if (!file.is_open())
		{
			LOG_ERROR("cannot open file 'config.json' for writing");
			return false;
		}
		file << j.dump(4);
		LOG_INFO("config saved successfully");
		return true;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR("failed to save config (%s)", e.what());
		return false;
	}
}