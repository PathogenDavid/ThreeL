#pragma once
#include "Window.h"

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
