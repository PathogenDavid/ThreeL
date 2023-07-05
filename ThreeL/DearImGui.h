#pragma once
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
}
