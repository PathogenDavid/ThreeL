#pragma once
#include "Vector3.h"
#include "Window.h"

#include <imgui.h>

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

public:
    DearImGui(GraphicsCore& graphicsCore, Window& window);

    void NewFrame();
    void Render(GraphicsContext& context);
    void RenderViewportWindows();
    
    ~DearImGui();

private:
    static std::optional<LRESULT> WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
};

namespace ImGui
{
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
}
