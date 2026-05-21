#include "OS-ImGui.h"
#include <cmath>

namespace OSImGui
{
    // Text rendering
    void OSImGui::Text(std::string Text, Vec2 Pos, ImColor Color, float FontSize, bool KeepCenter)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window)
            return;

        ImVec2 textSize = ImGui::CalcTextSize(Text.c_str());
        ImVec2 pos = KeepCenter ? ImVec2(Pos.x - textSize.x / 2, Pos.y - textSize.y / 2) : ImVec2(Pos.x, Pos.y);

        window->DrawList->AddText(ImGui::GetFont(), FontSize, pos, Color, Text.c_str());
    }

    void OSImGui::StrokeText(std::string Text, Vec2 Pos, ImColor Color, float FontSize, bool KeepCenter)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window)
            return;

        ImVec2 textSize = ImGui::CalcTextSize(Text.c_str());
        ImVec2 pos = KeepCenter ? ImVec2(Pos.x - textSize.x / 2, Pos.y - textSize.y / 2) : ImVec2(Pos.x, Pos.y);

        // OPTIMIZED: Drop Shadow instead of 9x outline (4.5x less vertices)
        ImU32 shadow_color = IM_COL32(0, 0, 0, 180); // Semi-transparent black
        ImVec2 shadow_pos = ImVec2(pos.x + 1, pos.y + 1); // Drop shadow offset
        
        // Draw shadow (1 call)
        window->DrawList->AddText(ImGui::GetFont(), FontSize, shadow_pos, shadow_color, Text.c_str());
        
        // Draw main text (1 call)
        window->DrawList->AddText(ImGui::GetFont(), FontSize, pos, Color, Text.c_str());
    }

// New optimized text function with minimal overhead
void OSImGui::DropShadowText(std::string Text, Vec2 Pos, ImColor Color, float FontSize, bool KeepCenter)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window)
            return;

        ImVec2 textSize = ImGui::CalcTextSize(Text.c_str());
        ImVec2 pos = KeepCenter ? ImVec2(Pos.x - textSize.x / 2, Pos.y - textSize.y / 2) : ImVec2(Pos.x, Pos.y);

        // Drop shadow (1 call) - 80% less vertices than StrokeText
        window->DrawList->AddText(ImGui::GetFont(), FontSize, ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0, 0, 0, 200), Text.c_str());
        
        // Main text (1 call)
        window->DrawList->AddText(ImGui::GetFont(), FontSize, pos, Color, Text.c_str());
    }

    // Rectangle
    void OSImGui::Rectangle(Vec2 Pos, Vec2 Size, ImColor Color, float Thickness, float Rounding)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window)
            return;

        window->DrawList->AddRect(ImVec2(Pos.x, Pos.y), ImVec2(Pos.x + Size.x, Pos.y + Size.y), Color, Rounding, 0, Thickness);
    }

    void OSImGui::RectangleFilled(Vec2 Pos, Vec2 Size, ImColor Color, float Rounding, int Nums)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window)
            return;

        window->DrawList->AddRectFilled(ImVec2(Pos.x, Pos.y), ImVec2(Pos.x + Size.x, Pos.y + Size.y), Color, Rounding);
    }

    // Line
    void OSImGui::Line(Vec2 From, Vec2 To, ImColor Color, float Thickness)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window)
            return;

        window->DrawList->AddLine(ImVec2(From.x, From.y), ImVec2(To.x, To.y), Color, Thickness);
    }

    // Circle
    void OSImGui::Circle(Vec2 Center, float Radius, ImColor Color, float Thickness, int Num)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window)
            return;

        window->DrawList->AddCircle(ImVec2(Center.x, Center.y), Radius, Color, Num, Thickness);
    }

    void OSImGui::CircleFilled(Vec2 Center, float Radius, ImColor Color, int Num)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window)
            return;

        window->DrawList->AddCircleFilled(ImVec2(Center.x, Center.y), Radius, Color, Num);
    }

    // Connect points
    void OSImGui::ConnectPoints(std::vector<Vec2> Points, ImColor Color, float Thickness)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window || Points.size() < 2)
            return;

        for (size_t i = 0; i < Points.size() - 1; i++)
        {
            window->DrawList->AddLine(ImVec2(Points[i].x, Points[i].y), ImVec2(Points[i + 1].x, Points[i + 1].y), Color, Thickness);
        }
    }

    // Arc
    void OSImGui::Arc(ImVec2 Center, float Radius, ImColor Color, float Thickness, float Angle_begin, float Angle_end, float Nums)
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (!window)
            return;

        window->DrawList->PathArcTo(Center, Radius, Angle_begin, Angle_end, (int)Nums);
        window->DrawList->PathStroke(Color, 0, Thickness);
    }

    // Checkbox (simplified versions)
    void OSImGui::MyCheckBox(const char* str_id, bool* v)
    {
        ImGui::Checkbox(str_id, v);
    }

    void OSImGui::MyCheckBox2(const char* str_id, bool* v)
    {
        MyCheckBox(str_id, v);
    }

    void OSImGui::MyCheckBox3(const char* str_id, bool* v)
    {
        MyCheckBox(str_id, v);
    }

    void OSImGui::MyCheckBox4(const char* str_id, bool* v)
    {
        MyCheckBox(str_id, v);
    }

    // Shadow effects (simplified)
    void OSImGui::ShadowRectFilled(Vec2 Pos, Vec2 Size, ImColor RectColor, ImColor ShadowColor, float ShadowThickness, Vec2 ShadowOffset, float Rounding)
    {
        RectangleFilled(Vec2(Pos.x + ShadowOffset.x, Pos.y + ShadowOffset.y), Size, ShadowColor, Rounding);
        RectangleFilled(Pos, Size, RectColor, Rounding);
    }

    void OSImGui::ShadowCircle(Vec2 Pos, float Radius, ImColor CircleColor, ImColor ShadowColor, float ShadowThickness, Vec2 ShadowOffset, int Num)
    {
        CircleFilled(Vec2(Pos.x + ShadowOffset.x, Pos.y + ShadowOffset.y), Radius, ShadowColor, Num);
        CircleFilled(Pos, Radius, CircleColor, Num);
    }

    // Slider
    bool OSImGui::SliderScalarEx1(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags)
    {
        return ImGui::SliderScalar(label, data_type, p_data, p_min, p_max, format, flags);
    }
}
