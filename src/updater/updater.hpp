#pragma once

#include <string>
#include <filesystem>

namespace updater
{
    // Запускает cs2-dumper.exe для генерации свежих дампов
    void run_dumper();

    // Загружает оффсеты из JSON-файлов в g_offsets
    // Возвращает true при успехе, false при ошибке
    bool load_json_offsets();
    bool load_dynamic_offsets();
}
