#pragma once
#include <cstddef>
#include <string>

namespace updater {
	// Загружает динамические оффсеты из output/client_dll.json и output/offsets.json
	// Возвращает true при успехе
	bool load_dynamic_offsets();
}
