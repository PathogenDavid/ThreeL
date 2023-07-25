#pragma once
#include "Vector3.h"
#include "Matrix4.h"
#include "Window.h"

#include <imgui.h>
#include <ImGuizmo.h>

struct GraphicsContext;
class GraphicsCore;
struct ImGuiContext;

class DearImGui
{
private:
    static DearImGui* s_Singleton;
    ImGuiContext* m_Context;
    WndProcHandle m_WndProcHandle;
    Window& m_Window;
    ImGuiStyle m_BaseStyle;
    ImGuizmo::Style m_BaseGizmoStyle;
    float m_LastScale = 1.f;

public:
    DearImGui(GraphicsCore& graphicsCore, Window& window);

    void NewFrame();
    void Render(GraphicsContext& context);
    void RenderViewportWindows();

    void ScaleUi(float scale);

    inline float DpiScale() const { return m_LastScale; }
    
    ~DearImGui();

private:
    static std::optional<LRESULT> WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
};

namespace ImGui
{
    //! Same as Begin except the window will be skipped when closed and it uses the if(Begin()) { End() } idiom used for non-windows in Dear ImGui.
    bool Begin2(const char* name, bool* p_open = nullptr, ImGuiWindowFlags flags = 0);

    inline void Text(std::string s) { ImGui::TextUnformatted(s.data(), s.data() + s.size()); }

    inline ImVec2 CalcTextSize(std::string s, bool hide_text_after_double_hash = false, float wrap_width = -1.0f)
    {
        return ImGui::CalcTextSize(s.data(), s.data() + s.size(), hide_text_after_double_hash, wrap_width);
    }

    inline bool SliderInt(const char* label, uint32_t* v, uint32_t v_min, uint32_t v_max, const char* format = "%u", ImGuiSliderFlags flags = 0)
    {
        return ImGui::SliderScalar(label, ImGuiDataType_U32, v, &v_min, &v_max, format, flags);
    }

    inline bool DragInt(const char* label, uint32_t* v, float v_speed = 1.f, uint32_t v_min = 0, uint32_t v_max = 0, const char* format = "%u", ImGuiSliderFlags flags = 0)
    {
        return ImGui::DragScalar(label, ImGuiDataType_U32, v, v_speed, &v_min, &v_max, format, flags);
    }

    inline float3 ColorConvertRGBtoHSV(const float3& rgb)
    {
        float3 hsv;
        ImGui::ColorConvertRGBtoHSV(rgb.x, rgb.y, rgb.z, hsv.x, hsv.y, hsv.z);
        return hsv;
    }

    inline float3 ColorConvertHSVtoRGB(const float3& hsv)
    {
        float3 rgb;
        ImGui::ColorConvertHSVtoRGB(hsv.x, hsv.y, hsv.z, rgb.x, rgb.y, rgb.z);
        return rgb;
    }

    inline void HudText(ImDrawList* draw_list, const ImVec2& pos, const std::string& text, float wrap_width = 0.f, ImU32 col = IM_COL32(255, 255, 255, 255), ImU32 shadow_col = IM_COL32(0, 0, 0, 255))
    {
        draw_list->AddText(nullptr, 0.f, ImVec2(pos.x - 1.f, pos.y - 1.f), shadow_col, text.data(), text.data() + text.size(), wrap_width);
        draw_list->AddText(nullptr, 0.f, ImVec2(pos.x + 0.f, pos.y - 1.f), shadow_col, text.data(), text.data() + text.size(), wrap_width);
        draw_list->AddText(nullptr, 0.f, ImVec2(pos.x + 1.f, pos.y - 1.f), shadow_col, text.data(), text.data() + text.size(), wrap_width);
        draw_list->AddText(nullptr, 0.f, ImVec2(pos.x - 1.f, pos.y + 0.f), shadow_col, text.data(), text.data() + text.size(), wrap_width);
        draw_list->AddText(nullptr, 0.f, ImVec2(pos.x + 1.f, pos.y + 0.f), shadow_col, text.data(), text.data() + text.size(), wrap_width);
        draw_list->AddText(nullptr, 0.f, ImVec2(pos.x - 1.f, pos.y + 1.f), shadow_col, text.data(), text.data() + text.size(), wrap_width);
        draw_list->AddText(nullptr, 0.f, ImVec2(pos.x + 0.f, pos.y + 1.f), shadow_col, text.data(), text.data() + text.size(), wrap_width);
        draw_list->AddText(nullptr, 0.f, ImVec2(pos.x + 1.f, pos.y + 1.f), shadow_col, text.data(), text.data() + text.size(), wrap_width);
        draw_list->AddText(nullptr, 0.f, pos, col, text.data(), text.data() + text.size(), wrap_width);
    }
}

namespace ImGuizmo
{
    inline bool Manipulate(const float4x4& view, const float4x4& projection, OPERATION operation, MODE mode, float4x4& matrix, float4x4* deltaMatrix = nullptr, const float3* snap = nullptr, const float* localBounds = nullptr, const float3* boundsSnap = nullptr)
    {
        return Manipulate(&view.m00, &projection.m00, operation, mode, &matrix.m00, (float*)deltaMatrix, (float*)snap, (float*)localBounds, (float*)boundsSnap);
    }

    inline bool Manipulate(const float4x4& view, const float4x4& projection, OPERATION operation, MODE mode, float3& position, const float3* snap = nullptr)
    {
        Assert((operation & TRANSLATE) != 0);
        float4x4 translation = float4x4::MakeTranslation(position);
        if (Manipulate(view, projection, operation, mode, translation, nullptr, snap))
        {
            position = float3(translation.m30, translation.m31, translation.m32);
            return true;
        }
        return false;
    }
}
