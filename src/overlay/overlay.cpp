#include "overlay.hpp"
#include "menu_config.hpp"
#include "../shared/game_data.hpp"
#include "../shared/view.hpp"
#include "../utils/config.hpp"
#include "../utils/skCrypter.h"
#include "../common.hpp"
#include "../utils/memory.hpp"
#include "../output/offsets.hpp"
#include <Windows.h>
#include <chrono>
#include <optional>
#include <string>
#include <algorithm>
#include <map>
#include <vector>
#include <cstdio>
#include <cmath>

// Bone connections for skeleton rendering - updated schema
static const std::pair<int, int> boneConnectionsFull[] = {
    // spine
    {1, 2},    // spine_lower -> spine_lower2
    {2, 23},   // spine_lower2 -> spine_middle2
    {23, 6},   // spine_middle2 -> spine_upper
    {6, 7},    // spine_upper -> neck

    // left arm
    {6, 9},    // spine_upper -> l_armpit
    {9, 10},   // l_armpit -> l_elbow
    {10, 11},  // l_elbow -> l_wrist

    // right arm
    {6, 13},   // spine_upper -> r_armpit
    {13, 14},  // r_armpit -> r_elbow
    {14, 15},  // r_elbow -> r_wrist

    // left leg
    {1, 17},   // spine_lower -> l_leg_joint2
    {17, 18},  // l_leg_joint2 -> l_knee
    {18, 19},  // l_knee -> l_foot

    // right leg
    {1, 20},   // spine_lower -> r_leg_joint2
    {20, 21},  // r_leg_joint2 -> r_knee
    {21, 22}   // r_knee -> r_foot
};

static constexpr size_t boneCountFull = sizeof(boneConnectionsFull) / sizeof(boneConnectionsFull[0]);

// Hitmarker / animation caches
struct HitMarkerEvent {
    double time;
    bool is_kill;
};
static std::vector<HitMarkerEvent> g_hitmarkers;
static std::map<int, int> g_health_cache; // previous hp per player index
static std::map<int, float> g_anim_hp;    // animated HP per player index

// Draw corner box with gradient fill and outlined corner lines
void DrawCornerBoxAndGradient(ImDrawList* draw_list, float x, float y, float w, float h, ImU32 color)
{
    // 1. Красивая градиентная заливка
    ImU32 col_top = IM_COL32(0, 0, 0, 0); // Полностью прозрачный верх
    ImU32 col_bottom = (color & 0x00FFFFFF) | (50 << 24); // Цвет ESP с альфа-каналом 50 для низа

    draw_list->AddRectFilledMultiColor(
        ImVec2(x, y), ImVec2(x + w, y + h),
        col_top, col_top, col_bottom, col_bottom
    );

    // 2. Отрисовка уголков (аккуратные и ограниченные)
    // Берем 1/4 от ширины/высоты, но НЕ БОЛЬШЕ 12 пикселей
    float line_w = std::min(w / 4.0f, 12.0f);
    float line_h = std::min(h / 4.0f, 12.0f);
    float thickness = 1.5f;

    // Лямбда для удобной отрисовки линии с черной тенью (как в премиум читах)
    auto draw_line_outlined = [&](ImVec2 start, ImVec2 end) {
        draw_list->AddLine(start, end, IM_COL32(0, 0, 0, 180), thickness + 2.0f); // Обводка
        draw_list->AddLine(start, end, color, thickness); // Основной цвет
    };

    // Верх-лево
    draw_line_outlined(ImVec2(x, y), ImVec2(x + line_w, y));
    draw_line_outlined(ImVec2(x, y), ImVec2(x, y + line_h));
    // Верх-право
    draw_line_outlined(ImVec2(x + w, y), ImVec2(x + w - line_w, y));
    draw_line_outlined(ImVec2(x + w, y), ImVec2(x + w, y + line_h));
    // Низ-лево
    draw_line_outlined(ImVec2(x, y + h), ImVec2(x + line_w, y + h));
    draw_line_outlined(ImVec2(x, y + h), ImVec2(x, y + h - line_h));
    // Низ-право
    draw_line_outlined(ImVec2(x + w, y + h), ImVec2(x + w - line_w, y + h));
    draw_line_outlined(ImVec2(x + w, y + h), ImVec2(x + w, y + h - line_h));
}


// Render molotov/inferno area (convex hull of fire points)
void RenderMolotovInferno(uintptr_t entity_ptr, const std::array<float, 16>& view_matrix, float screen_width, float screen_height)
{
    // TODO: Подставить реальные оффсеты из свежего дампа
    // Например: int fire_count = m_memory->read_t<int>(entity_ptr + offsets::m_fireCount);
    int fire_count = 0;
    if (fire_count <= 0 || fire_count > 64) return;

    std::vector<ImVec2> screen_points;

    for (int i = 0; i < fire_count; ++i)
    {
        // TODO: Прочитать статус горения и позицию конкретного огонька
        // bool is_burning = m_memory->read_t<bool>(entity_ptr + offsets::m_bFireIsBurning + i);
        // vector_t fire_pos = m_memory->read_t<vector_t>(entity_ptr + offsets::m_firePositions + (i * sizeof(vector_t)));
        bool is_burning = false; // ЗАГЛУШКА
        vector_t fire_pos = {0,0,0}; // ЗАГЛУШКА

        if (is_burning)
        {
            vector_t screen_pos;
            if (shared::world_to_screen(fire_pos, screen_pos, view_matrix, screen_width, screen_height)) {
                screen_points.push_back(ImVec2(screen_pos.m_x, screen_pos.m_y));
            }
        }
    }

    if (screen_points.size() >= 3) {
        // Build hull
        std::vector<ImVec2> hull = shared::build_convex_hull(screen_points);

        ImU32 fill_color = IM_COL32(255, 80, 0, 60);
        ImGui::GetBackgroundDrawList()->AddConvexPolyFilled(hull.data(), hull.size(), fill_color);

        ImU32 outline_color = IM_COL32(255, 120, 0, 255);
        ImGui::GetBackgroundDrawList()->AddPolyline(hull.data(), hull.size(), outline_color, ImDrawFlags_Closed, 2.0f);
    }
}

// Render smoke grenade ring + timer
void RenderSmokeGrenade(uintptr_t entity_ptr, float global_time, const std::array<float, 16>& view_matrix, float screen_width, float screen_height)
{
    // TODO: Прочитать оффсеты смока
    // bool did_smoke = m_memory->read_t<bool>(entity_ptr + offsets::m_bDidSmokeEffect);
    // int tick_begin = m_memory->read_t<int>(entity_ptr + offsets::m_nSmokeEffectTickBegin);
    // vector_t smoke_origin = m_memory->read_t<vector_t>(entity_ptr + m_pGameSceneNode + m_vecAbsOrigin);

    bool did_smoke = false; // ЗАГЛУШКА
    int tick_begin = 0; // ЗАГЛУШКА
    vector_t smoke_origin = {0,0,0}; // ЗАГЛУШКА

    if (!did_smoke) return;

    float start_time = tick_begin * (1.0f / 64.0f);
    float time_alive = global_time - start_time;
    float time_left = 18.0f - time_alive;

    if (time_left <= 0.0f || time_left > 18.0f) return;

    float smoke_radius = 144.0f;
    std::vector<ImVec2> ring_points;
    int segments = 32;

    for (int i = 0; i <= segments; i++) {
        float angle = (i / (float)segments) * 3.14159f * 2.0f;
        vector_t point_3d = {
            smoke_origin.m_x + cosf(angle) * smoke_radius,
            smoke_origin.m_y + sinf(angle) * smoke_radius,
            smoke_origin.m_z
        };

        vector_t point_2d;
        if (shared::world_to_screen(point_3d, point_2d, view_matrix, screen_width, screen_height)) {
            ring_points.push_back(ImVec2(point_2d.m_x, point_2d.m_y));
        }
    }

    if (ring_points.size() == (size_t)segments + 1) {
        ImU32 smoke_color = IM_COL32(180, 200, 255, 100);
        ImGui::GetBackgroundDrawList()->AddPolyline(ring_points.data(), ring_points.size(), smoke_color, 0, 2.0f);
        ImGui::GetBackgroundDrawList()->AddConvexPolyFilled(ring_points.data(), ring_points.size(), IM_COL32(150, 180, 255, 30));
    }

    vector_t center_2d;
    if (shared::world_to_screen(smoke_origin, center_2d, view_matrix, screen_width, screen_height)) {
        char timer_text[16];
        snprintf(timer_text, sizeof(timer_text), "%.1f", time_left);

        ImU32 text_col = (time_left < 3.0f) ? IM_COL32(255, 50, 50, 255) : IM_COL32(255, 255, 255, 255);
        ImVec2 text_size = ImGui::CalcTextSize(timer_text);

        ImGui::GetBackgroundDrawList()->AddText(ImVec2(center_2d.m_x - text_size.x / 2 + 1, center_2d.m_y - text_size.y / 2 + 1), IM_COL32(0,0,0,255), timer_text);
        ImGui::GetBackgroundDrawList()->AddText(ImVec2(center_2d.m_x - text_size.x / 2, center_2d.m_y - text_size.y / 2), text_col, timer_text);
    }
}

// Render hitmarkers (center-screen)
void RenderHitmarkers(ImDrawList* draw_list, ImVec2 screen_center)
{
    double current_time = ImGui::GetTime();
    double duration = 0.5; // seconds

    for (auto it = g_hitmarkers.begin(); it != g_hitmarkers.end(); ) {
        double time_alive = current_time - it->time;
        if (time_alive > duration) {
            it = g_hitmarkers.erase(it);
            continue;
        }

        float alpha_fade = static_cast<float>(1.0 - (time_alive / duration));
        int alpha = static_cast<int>(255 * alpha_fade);
        ImU32 color = it->is_kill ? IM_COL32(255, 50, 50, alpha) : IM_COL32(255, 255, 255, alpha);

        float size = 6.0f + static_cast<float>(time_alive * 8.0);
        float gap = 4.0f;
        float thickness = 1.5f;

        draw_list->AddLine(ImVec2(screen_center.x - gap, screen_center.y - gap), ImVec2(screen_center.x - size, screen_center.y - size), color, thickness);
        draw_list->AddLine(ImVec2(screen_center.x + gap, screen_center.y - gap), ImVec2(screen_center.x + size, screen_center.y - size), color, thickness);
        draw_list->AddLine(ImVec2(screen_center.x - gap, screen_center.y + gap), ImVec2(screen_center.x - size, screen_center.y + size), color, thickness);
        draw_list->AddLine(ImVec2(screen_center.x + gap, screen_center.y + gap), ImVec2(screen_center.x + size, screen_center.y + size), color, thickness);

        ++it;
    }
}

// ESP Render function - draws boxes around enemies
void RenderESP()
{
    // Get game state from double-buffered shared memory (lock-free)
    auto players = shared::g_double_buffered_state.read_players();
    auto local = shared::g_double_buffered_state.read_local();

    if (!local.is_valid)
        return;

    // === 1. GLOBAL ViewMatrix READ (Fresh read every frame for camera smoothness) ===
    extern const std::unique_ptr<c_memory> m_memory;
    static uintptr_t client_base = 0;

    if (client_base == 0)
    {
        auto module_info = m_memory->get_module_info(CLIENT_DLL);
        if (module_info.first.has_value())
            client_base = module_info.first.value();
    }

    std::array<float, 16> view_matrix{};
    bool vm_valid = false;
    if (client_base != 0)
    {
        struct view_matrix_t { float matrix[16]; };
        auto vm = m_memory->read_t<view_matrix_t>(client_base + g_offsets::view_matrix);
        std::copy(std::begin(vm.matrix), std::end(vm.matrix), view_matrix.begin());
        vm_valid = true;
    }

    if (!vm_valid)
        return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (!drawList)
        drawList = ImGui::GetForegroundDrawList();

    // === 2. CACHED SCREEN SIZE (Single ImGui API call per frame) ===
    static float cached_screenW = 0.0f;
    static float cached_screenH = 0.0f;
    static uint64_t last_screen_update = 0;

    const auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    if (current_time - last_screen_update > 100 || cached_screenW == 0.0f) {
        cached_screenW = ImGui::GetIO().DisplaySize.x;
        cached_screenH = ImGui::GetIO().DisplaySize.y;
        last_screen_update = current_time;
    }

    float screenW = cached_screenW;
    float screenH = cached_screenH;

    // Render each player
    for (const auto& player : players)
    {
        if (player.team == local.team && !esp::g_menu_config.ShowTeamESP)
            continue;

        if (player.is_dead)
            continue;

        bool use_extrapolation = (player.velocity.length_sqr() > 10.0f) && (esp::g_menu_config.ExtrapolationAmount > 0.0f);

        // === SMART FRUSTUM CULLING ===
        vector_t screenPosFeet, screenPosHead;
        bool feet_on_screen = shared::world_to_screen(player.get_lerp_position(), screenPosFeet, view_matrix, screenW, screenH);
        bool head_on_screen = shared::world_to_screen(player.head_pos, screenPosHead, view_matrix, screenW, screenH);

        if (!feet_on_screen && !head_on_screen)
            continue;

        // === DYNAMIC BONE-BASED BOUNDING BOX ===
        // Формируем бокс ИДЕАЛЬНО по экстраполированному скелету. 
        // Это полностью исключает тряску текста и боксов отдельно от модельки.
        float boxX = 0, boxY = 0, boxWidth = 0, boxHeight = 0;
        bool valid_box = false;

        if (player.bone_mask != 0) {
            float minX = 99999.0f, minY = 99999.0f;
            float maxX = -99999.0f, maxY = -99999.0f;
            int valid_bones = 0;

            for (size_t i = 0; i < _countof(boneConnectionsFull); i++) {
                int bone_ids[] = { boneConnectionsFull[i].first, boneConnectionsFull[i].second };
                for (int b : bone_ids) {
                    if (!player.has_bone(b)) continue;

                    vector_t bone_pos = player.get_bone(b);
                    if (use_extrapolation) bone_pos = player.get_extrapolated_bone(bone_pos);

                    vector_t screen_pos;
                    if (shared::world_to_screen(bone_pos, screen_pos, view_matrix, screenW, screenH)) {
                        minX = std::min(minX, screen_pos.m_x);
                        minY = std::min(minY, screen_pos.m_y);
                        maxX = std::max(maxX, screen_pos.m_x);
                        maxY = std::max(maxY, screen_pos.m_y);
                        valid_bones++;
                    }
                }
            }

            // Захватываем саму голову с запасом, чтобы бокс не резал макушку
            vector_t head = player.head_pos;
            if (use_extrapolation) head = player.get_extrapolated_bone(head);
            vector_t head_screen;
            if (shared::world_to_screen(head, head_screen, view_matrix, screenW, screenH)) {
                minX = std::min(minX, head_screen.m_x - 6.0f);
                maxX = std::max(maxX, head_screen.m_x + 6.0f);
                minY = std::min(minY, head_screen.m_y - 8.0f);
                valid_bones++;
            }

            if (valid_bones > 0) {
                // Добавляем небольшой Padding (отступы), чтобы бокс не лип к пикселям модели
                float width = maxX - minX;
                float height = maxY - minY;

                // Фикс для худых боксов, когда противник стоит боком
                if (width < height * 0.25f) width = height * 0.25f;

                boxX = minX - (width * 0.15f);
                boxY = minY - (height * 0.05f);
                boxWidth = width * 1.30f;
                boxHeight = height * 1.10f;
                valid_box = true;
            }
        }

        // Если кости не прочитались (редко, но бывает) - запасной метод
        if (!valid_box) {
            shared::PlayerData extrapolated_player = player;
            if (use_extrapolation) {
                extrapolated_player.world_pos = player.get_extrapolated_bone(player.get_lerp_position());
                extrapolated_player.head_pos = player.get_extrapolated_bone(player.head_pos);
            }
            else {
                extrapolated_player.world_pos = player.get_lerp_position();
            }

            if (!shared::calculate_2d_box_fast(extrapolated_player, boxX, boxY, boxWidth, boxHeight, view_matrix, screenW, screenH))
                continue;
        }

        // Determine color based on health
        ImU32 boxColor;
        if (player.health > 75)
            boxColor = IM_COL32(0, 255, 0, 255);      // Green
        else if (player.health > 40)
            boxColor = IM_COL32(255, 255, 0, 255);  // Yellow
        else
            boxColor = IM_COL32(255, 0, 0, 255);    // Red

        if (esp::g_menu_config.BoxColorEnemy != 0xFF0000FF)
            boxColor = esp::g_menu_config.BoxColorEnemy;

        // === Draw 2D Box ===
        if (esp::g_menu_config.ShowBoxESP)
        {
            // Если включена обычная заливка, рисуем её под градиентом
            if (esp::g_menu_config.BoxFill)
            {
                ImVec2 rectMin(boxX, boxY);
                ImVec2 rectMax(boxX + boxWidth, boxY + boxHeight);
                drawList->AddRectFilled(rectMin, rectMax, esp::g_menu_config.BoxFillColor);
            }

            // Рисуем уголки + градиентную заливку
            DrawCornerBoxAndGradient(drawList, boxX, boxY, boxWidth, boxHeight, boxColor);
        }

        // === Draw Health Bar (animated) ===
        if (esp::g_menu_config.ShowHealthBar)
        {
            // Инициализация кеша анимированного ХП
            if (g_anim_hp.find(player.index) == g_anim_hp.end()) {
                g_anim_hp[player.index] = static_cast<float>(player.health);
            }

            // Lerp к реальному ХП
            g_anim_hp[player.index] += (static_cast<float>(player.health) - g_anim_hp[player.index]) * ImGui::GetIO().DeltaTime * 10.0f;

            float health_frac = std::clamp(g_anim_hp[player.index] / 100.0f, 0.0f, 1.0f);
            float bar_height = boxHeight * health_frac;

            ImU32 hp_color = IM_COL32(
                static_cast<int>(255 * (1.0f - health_frac)),
                static_cast<int>(255 * health_frac),
                0,
                255
            );

            if (esp::g_menu_config.HealthBarVertical)
            {
                float barX = boxX - esp::g_menu_config.HealthBarWidth - 5.0f;
                // Background
                drawList->AddRectFilled(ImVec2(barX - 1.0f, boxY - 1.0f), ImVec2(barX + esp::g_menu_config.HealthBarWidth + 1.0f, boxY + boxHeight + 1.0f), IM_COL32(0,0,0,180));
                // Animated fill
                drawList->AddRectFilled(ImVec2(barX, boxY + boxHeight - bar_height), ImVec2(barX + esp::g_menu_config.HealthBarWidth, boxY + boxHeight), hp_color);

                // Digital text
                std::string hp_text = std::to_string(static_cast<int>(std::round(g_anim_hp[player.index])));
                ImVec2 textSize = ImGui::CalcTextSize(hp_text.c_str());
                ImVec2 textPos(barX + (esp::g_menu_config.HealthBarWidth * 0.5f) - (textSize.x * 0.2f), boxY + boxHeight - bar_height - (textSize.y * 0.2f));
                drawList->AddText(ImVec2(textPos.x - 1, textPos.y), IM_COL32(0,0,0,255), hp_text.c_str());
                drawList->AddText(ImVec2(textPos.x + 1, textPos.y), IM_COL32(0,0,0,255), hp_text.c_str());
                drawList->AddText(textPos, IM_COL32(255,255,255,255), hp_text.c_str());
            }
            else
            {
                float barX = boxX;
                float barY = boxY + boxHeight + 3.0f;
                float barWidth = boxWidth * health_frac;

                drawList->AddRectFilled(ImVec2(barX - 1.0f, barY - 1.0f), ImVec2(barX + boxWidth + 1.0f, barY + esp::g_menu_config.HealthBarWidth + 1.0f), IM_COL32(0,0,0,180));
                drawList->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barWidth, barY + esp::g_menu_config.HealthBarWidth), hp_color);
            }
        }

        // === Money + Weapon display (маленький шрифт, колонка справа) ===
        {
            char money_str[32];
            snprintf(money_str, sizeof(money_str), "$%d", player.money);
            ImU32 money_color = IM_COL32(133, 187, 101, 255);

            float small_font_size = ImGui::GetFontSize() * 0.8f;
            ImVec2 text_pos(boxX + boxWidth + 4.0f, boxY);

            drawList->AddText(ImGui::GetFont(), small_font_size, text_pos, money_color, money_str);

            // Если включено отображение оружия, рисуем его под суммой
            if (esp::g_menu_config.ShowWeapon && !player.weapon_name.empty()) {
                ImVec2 weapon_pos(text_pos.x, text_pos.y + small_font_size + 2.0f);
                ImU32 wep_color = IM_COL32(200, 200, 200, 220);
                drawList->AddText(ImGui::GetFont(), small_font_size, weapon_pos, wep_color, player.weapon_name.c_str());
            }
        }

        // === Hitmarker detection (compare с предыдущим хп) ===
        if (g_health_cache.find(player.index) != g_health_cache.end()) {
            int prev_hp = g_health_cache[player.index];
            if (player.health < prev_hp && player.health >= 0) {
                bool is_kill = (player.health == 0);
                g_hitmarkers.push_back({ ImGui::GetTime(), is_kill });
            }
        }
        g_health_cache[player.index] = player.health;

        // === Name & Weapon ===
        if (esp::g_menu_config.ShowName && !player.name.empty())
        {
            ImVec2 textPos(boxX + boxWidth * 0.5f, boxY - 15.0f);
            ImVec2 textSize = ImGui::CalcTextSize(player.name.c_str());
            textPos.x -= textSize.x * 0.5f;
            drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), player.name.c_str());
        }



        // === FULL SKELETON RENDERING ===
        if (esp::g_menu_config.ShowSkeleton && player.bone_mask != 0)
        {
            ImU32 skeletonColor = esp::g_menu_config.SkeletonColor;
            constexpr float skeleton_thickness = 1.5f;

            for (size_t i = 0; i < _countof(boneConnectionsFull); i++)
            {
                int boneIdx1 = boneConnectionsFull[i].first;
                int boneIdx2 = boneConnectionsFull[i].second;

                if (!player.has_bone(boneIdx1) || !player.has_bone(boneIdx2))
                    continue;

                vector_t bone1_pos = player.get_bone(boneIdx1);
                vector_t bone2_pos = player.get_bone(boneIdx2);

                if (use_extrapolation)
                {
                    bone1_pos = player.get_extrapolated_bone(bone1_pos);
                    bone2_pos = player.get_extrapolated_bone(bone2_pos);
                }

                vector_t screenPos1, screenPos2;

                if (shared::world_to_screen(bone1_pos, screenPos1, view_matrix, screenW, screenH) &&
                    shared::world_to_screen(bone2_pos, screenPos2, view_matrix, screenW, screenH))
                {
                    ImVec2 p1(screenPos1.m_x, screenPos1.m_y);
                    ImVec2 p2(screenPos2.m_x, screenPos2.m_y);
                    drawList->AddLine(p1, p2, skeletonColor, skeleton_thickness);
                }
            }
        }
    }
}

// Render ImGui menu
void RenderMenu()
{
    if (!esp::g_menu_config.ShowMenu)
        return;

    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);

    ImGui::Begin("CS2 WebRadar ESP", &esp::g_menu_config.ShowMenu);

    if (ImGui::BeginTabBar("ESPSettings"))
    {
        if (ImGui::BeginTabItem("General"))
        {
            ImGui::Checkbox("Enable ESP", &esp::g_menu_config.ESPEnabled);
            ImGui::Checkbox("Show Boxes", &esp::g_menu_config.ShowBoxESP);
            ImGui::Checkbox("Show Skeleton", &esp::g_menu_config.ShowSkeleton);
            ImGui::Checkbox("Show Health Bar", &esp::g_menu_config.ShowHealthBar);
            ImGui::Checkbox("Show Name", &esp::g_menu_config.ShowName);
            ImGui::Checkbox("Show Weapon", &esp::g_menu_config.ShowWeapon);
            ImGui::Separator();
            if (esp::g_menu_config.MonitorRefreshRate > 0) {
                ImGui::Text("Overlay FPS: %d Hz (auto-detected)", esp::g_menu_config.MonitorRefreshRate);
            }
            else {
                ImGui::Text("Overlay FPS: 60 Hz (fallback)");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Colors"))
        {
            float colEnemy[4] = {
                ((esp::g_menu_config.BoxColorEnemy >> 0) & 0xFF) / 255.0f,
                ((esp::g_menu_config.BoxColorEnemy >> 8) & 0xFF) / 255.0f,
                ((esp::g_menu_config.BoxColorEnemy >> 16) & 0xFF) / 255.0f,
                ((esp::g_menu_config.BoxColorEnemy >> 24) & 0xFF) / 255.0f
            };
            if (ImGui::ColorEdit4("Enemy Box Color", colEnemy))
            {
                esp::g_menu_config.BoxColorEnemy =
                    ((uint32_t)(colEnemy[0] * 255) << 0) |
                    ((uint32_t)(colEnemy[1] * 255) << 8) |
                    ((uint32_t)(colEnemy[2] * 255) << 16) |
                    ((uint32_t)(colEnemy[3] * 255) << 24);
            }

            float colTeammate[4] = {
                ((esp::g_menu_config.BoxColorTeammate >> 0) & 0xFF) / 255.0f,
                ((esp::g_menu_config.BoxColorTeammate >> 8) & 0xFF) / 255.0f,
                ((esp::g_menu_config.BoxColorTeammate >> 16) & 0xFF) / 255.0f,
                ((esp::g_menu_config.BoxColorTeammate >> 24) & 0xFF) / 255.0f
            };
            if (ImGui::ColorEdit4("Teammate Box Color", colTeammate))
            {
                esp::g_menu_config.BoxColorTeammate =
                    ((uint32_t)(colTeammate[0] * 255) << 0) |
                    ((uint32_t)(colTeammate[1] * 255) << 8) |
                    ((uint32_t)(colTeammate[2] * 255) << 16) |
                    ((uint32_t)(colTeammate[3] * 255) << 24);
            }

            float colSkeleton[4] = {
                ((esp::g_menu_config.SkeletonColor >> 0) & 0xFF) / 255.0f,
                ((esp::g_menu_config.SkeletonColor >> 8) & 0xFF) / 255.0f,
                ((esp::g_menu_config.SkeletonColor >> 16) & 0xFF) / 255.0f,
                ((esp::g_menu_config.SkeletonColor >> 24) & 0xFF) / 255.0f
            };
            if (ImGui::ColorEdit4("Skeleton Color", colSkeleton))
            {
                esp::g_menu_config.SkeletonColor =
                    ((uint32_t)(colSkeleton[0] * 255) << 0) |
                    ((uint32_t)(colSkeleton[1] * 255) << 8) |
                    ((uint32_t)(colSkeleton[2] * 255) << 16) |
                    ((uint32_t)(colSkeleton[3] * 255) << 24);
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Style"))
        {
            ImGui::SliderFloat("Box Thickness", &esp::g_menu_config.BoxThickness, 0.5f, 5.0f);
            ImGui::Checkbox("Fill Box", &esp::g_menu_config.BoxFill);
            ImGui::Checkbox("Vertical Health Bar", &esp::g_menu_config.HealthBarVertical);
            ImGui::SliderFloat("Health Bar Width", &esp::g_menu_config.HealthBarWidth, 2.0f, 10.0f);
            ImGui::SliderFloat("Extrapolation (Smoothness)", &esp::g_menu_config.ExtrapolationAmount, 0.0f, 2.0f);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Triggerbot"))
        {
            ImGui::Checkbox("Enable Triggerbot", &esp::g_menu_config.TriggerBotEnabled);
            ImGui::SliderInt("Triggerbot Delay (ms)", &esp::g_menu_config.TriggerDelay, 20, 300);
            ImGui::Separator();

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Separator();

    if (ImGui::Button("Save Config"))
    {
        LOG_INFO("Config save requested");
    }

    ImGui::Text("Hotkeys:");
    ImGui::Text("INSERT - Toggle ESP");
    ImGui::Text("PGDN - Toggle Menu");

    ImGui::End();
}

// Handle hotkeys
void HandleHotkeys()
{
    static bool insertPressed = false;
    static bool pgdnPressed = false;

    if (GetAsyncKeyState(VK_INSERT) & 0x8000)
    {
        if (!insertPressed)
        {
            esp::g_menu_config.ESPEnabled = !esp::g_menu_config.ESPEnabled;
            insertPressed = true;
        }
    }
    else insertPressed = false;

    if (GetAsyncKeyState(VK_NEXT) & 0x8000)
    {
        if (!pgdnPressed)
        {
            esp::g_menu_config.ShowMenu = !esp::g_menu_config.ShowMenu;
            pgdnPressed = true;
        }
    }
    else pgdnPressed = false;
}

// Render sniper crosshair for AWP and SSG08
void RenderSniperCrosshair()
{
    auto local = shared::g_double_buffered_state.read_local();

    if (!local.is_valid)
        return;

    bool is_sniper = local.weapon_name.find("awp") != std::string::npos ||
        local.weapon_name.find("ssg08") != std::string::npos;

    if (!is_sniper)
        return;

    ImVec2 screen_center = ImGui::GetIO().DisplaySize;
    screen_center.x *= 0.5f;
    screen_center.y *= 0.5f;

    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

    const float crosshair_size = 8.0f;
    const float crosshair_thickness = 1.5f;
    const ImU32 crosshair_color = IM_COL32(128, 0, 255, 255);

    draw_list->AddLine(
        ImVec2(screen_center.x - crosshair_size, screen_center.y),
        ImVec2(screen_center.x + crosshair_size, screen_center.y),
        crosshair_color, crosshair_thickness);

    draw_list->AddLine(
        ImVec2(screen_center.x, screen_center.y - crosshair_size),
        ImVec2(screen_center.x, screen_center.y + crosshair_size),
        crosshair_color, crosshair_thickness);

    draw_list->AddCircleFilled(screen_center, 2.0f, crosshair_color);
}

// Main render callback for ImGui
void RenderLoop()
{
    HandleHotkeys();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

    ImGui::Begin("Overlay", nullptr,
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoInputs |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    if (esp::g_menu_config.ESPEnabled)
    {
        RenderESP();
    }

    RenderSniperCrosshair();
    // Render hitmarkers on top of everything
    ImVec2 screen_center = ImGui::GetIO().DisplaySize;
    screen_center.x *= 0.5f;
    screen_center.y *= 0.5f;
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    RenderHitmarkers(fg, screen_center);
    ImGui::End();
    RenderMenu();
}

void OverlayThreadFunc()
{
    try
    {
        printf("[OVERLAY] Initializing ImGui overlay...\n");
        OSImGui::OSImGui::get().AttachAnotherWindow("Counter-Strike 2", "SDL_app", RenderLoop);
    }
    catch (OSImGui::OSException& e)
    {
        printf("[OVERLAY ERROR] %s\n", e.what());
    }
    catch (const std::exception& e)
    {
        printf("[OVERLAY ERROR] Standard exception: %s\n", e.what());
    }
    catch (...)
    {
        printf("[OVERLAY ERROR] Unknown exception occurred\n");
    }
}

void InitOverlay()
{
    std::thread overlayThread(OverlayThreadFunc);
    overlayThread.detach();
    printf("[OVERLAY] Overlay thread started (detached)\n");
}