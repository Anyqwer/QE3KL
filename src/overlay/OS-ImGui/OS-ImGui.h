#pragma once
#include "OS-ImGui_External.h"

namespace OSImGui
{
    class OSImGui : public OSImGui_External, public Singleton<OSImGui>
    {
    public:
        // Text
        void Text(std::string Text, Vec2 Pos, ImColor Color, float FontSize = 15, bool KeepCenter = false);
        // Stroke text
        void StrokeText(std::string Text, Vec2 Pos, ImColor Color, float FontSize = 15, bool KeepCenter = false);
        // Optimized drop shadow text (4.5x less vertices than StrokeText)
        void DropShadowText(std::string Text, Vec2 Pos, ImColor Color, float FontSize = 15, bool KeepCenter = false);
        // Rectangle
        void Rectangle(Vec2 Pos, Vec2 Size, ImColor Color, float Thickness, float Rounding = 0);
        void RectangleFilled(Vec2 Pos, Vec2 Size, ImColor Color, float Rounding = 0, int Nums = 15);
        // Line
        void Line(Vec2 From, Vec2 To, ImColor Color, float Thickness);
        // Circle
        void Circle(Vec2 Center, float Radius, ImColor Color, float Thickness, int Num = 50);
        void CircleFilled(Vec2 Center, float Radius, ImColor Color, int Num = 50);
        // Connect points
        void ConnectPoints(std::vector<Vec2> Points, ImColor Color, float Thickness);
        // Arc
        void Arc(ImVec2 Center, float Radius, ImColor Color, float Thickness, float Aangle_begin, float Angle_end, float Nums = 15);
        // Checkbox
        void MyCheckBox(const char* str_id, bool* v);
        void MyCheckBox2(const char* str_id, bool* v);
        void MyCheckBox3(const char* str_id, bool* v);
        void MyCheckBox4(const char* str_id, bool* v);
        // Shadow rect
        void ShadowRectFilled(Vec2 Pos, Vec2 Size, ImColor RectColor, ImColor ShadowColor, float ShadowThickness, Vec2 ShadowOffset, float Rounding = 0);
        // Shadow circle
        void ShadowCircle(Vec2 Pos, float Radius, ImColor CircleColor, ImColor ShadowColor, float ShadowThickness, Vec2 ShadowOffset, int Num = 30);
        // Slider
        bool SliderScalarEx1(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags);
    };
}

inline OSImGui::OSImGui& Gui = OSImGui::OSImGui::get();
