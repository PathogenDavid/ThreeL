#include "pch.h"
#include "DearImGui.h"

#include "GraphicsContext.h"
#include "GraphicsCore.h"
#include "SwapChain.h"
#include "Window.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <ImGuizmo.h>

DearImGui* DearImGui::s_Singleton = nullptr;

DearImGui::DearImGui(GraphicsCore& graphicsCore, Window& window)
    : m_Window(window)
{
    Assert(s_Singleton == nullptr);
    s_Singleton = this;

    //---------------------------------------------------------------------------------------------------------
    // Set up Dear ImGui
    //---------------------------------------------------------------------------------------------------------
    IMGUI_CHECKVERSION();
    m_Context = ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // More annoying than anything since it requires unfocusing the window to move the camera
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.IniFilename = nullptr;

    //---------------------------------------------------------------------------------------------------------
    // Configure Style
    //---------------------------------------------------------------------------------------------------------
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();

    // Show tooltips on disabled items
    style.HoverFlagsForTooltipMouse |= ImGuiHoveredFlags_AllowWhenDisabled;
    style.HoverFlagsForTooltipNav |= ImGuiHoveredFlags_AllowWhenDisabled;

    // Disable window rounding and window background alpha since it doesn't play nice with multiple viewports
    style.WindowRounding = 0.f;
    style.Colors[ImGuiCol_WindowBg].w = 1.f;

    m_BaseStyle = style;

    // ImGuizmo style
    ImGuizmo::Style& gizmoStyle = ImGuizmo::GetStyle();
    ImGuizmo::AllowAxisFlip(false);
    ImGuizmo::SetGizmoSizeClipSpace(0.15f);

    gizmoStyle.Colors[ImGuizmo::PLANE_X] = gizmoStyle.Colors[ImGuizmo::DIRECTION_X] = ImVec4(237.f / 255.f, 56.f / 255.f, 80.f / 255.f, 1.f);
    gizmoStyle.Colors[ImGuizmo::PLANE_Y] = gizmoStyle.Colors[ImGuizmo::DIRECTION_Y] = ImVec4(131.f / 255.f, 206.f / 255.f, 24.f / 255.f, 1.f);
    gizmoStyle.Colors[ImGuizmo::PLANE_Z] = gizmoStyle.Colors[ImGuizmo::DIRECTION_Z] = ImVec4(48.f / 255.f, 133.f / 255.f, 234.f / 255.f, 1.f);
    gizmoStyle.Colors[ImGuizmo::PLANE_X].w = 0.38f;
    gizmoStyle.Colors[ImGuizmo::PLANE_Y].w = 0.38f;
    gizmoStyle.Colors[ImGuizmo::PLANE_Z].w = 0.38f;
    gizmoStyle.Colors[ImGuizmo::SELECTION] = ImVec4(240.f / 255.f, 240.f / 255.f, 157.f / 255.f, 0.541f);

    m_BaseGizmoStyle = gizmoStyle;

    // Apply initial scaling
    UINT dpi = GetDpiForWindow(window.Hwnd());
    Assert(dpi != 0);
    if (dpi != 0)
    { ScaleUi((float)dpi / (float)USER_DEFAULT_SCREEN_DPI); }

    //---------------------------------------------------------------------------------------------------------
    // Initialize platform/renderer backends
    //---------------------------------------------------------------------------------------------------------
    ImGui_ImplWin32_Init(window.Hwnd());
    auto fontDescriptor = graphicsCore.ResourceDescriptorManager().AllocateUninitializedResidentDescriptor();
    ImGui_ImplDX12_Init
    (
        graphicsCore.Device(),
        SwapChain::BACK_BUFFER_COUNT,
        SwapChain::BACK_BUFFER_FORMAT,
        graphicsCore.ResourceDescriptorManager().GpuHeap(),
        std::get<0>(fontDescriptor),
        std::get<1>(fontDescriptor)
    );

    m_WndProcHandle = window.AddWndProc(DearImGui::WndProc);
}

void DearImGui::NewFrame()
{
    Assert(ImGui::GetCurrentContext() == m_Context);
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Set up ImGuizmo for this frame
    // We don't bother calling ImGuizmo::BeginFrame here
    // All it does is submit a dummy window for rendering gizmos to and we're drawing them directly to the background drawlist
    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList(mainViewport));
    ImGuizmo::SetRect(0.f, 0.f, mainViewport->Size.x, mainViewport->Size.y);
}

void DearImGui::Render(GraphicsContext& context)
{
    Assert(ImGui::GetCurrentContext() == m_Context);
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), context.CommandList());
}

void DearImGui::RenderViewportWindows()
{
    if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault(nullptr, nullptr);
    }
}

void DearImGui::ScaleUi(float scale)
{
    ImGuiStyle& style = ImGui::GetStyle();
    style = m_BaseStyle;
    style.ScaleAllSizes(scale);
    ImGui::GetIO().FontGlobalScale = scale;

    ImGuizmo::Style& gizmoStyle = ImGuizmo::GetStyle();
    gizmoStyle = m_BaseGizmoStyle;
    gizmoStyle.TranslationLineThickness *= scale;
    gizmoStyle.TranslationLineArrowSize *= scale;
    gizmoStyle.RotationLineThickness *= scale;
    gizmoStyle.RotationOuterLineThickness *= scale;
    gizmoStyle.ScaleLineThickness *= scale;
    gizmoStyle.ScaleLineCircleSize *= scale;
    gizmoStyle.HatchedAxisLineThickness *= scale;
    gizmoStyle.CenterCircleSize *= scale;

    // Reposition and resize windows so their relative positions and sizes remain the same
    // Dear ImGui doesn't have super great support for handling DPI changes, so this is only best effort
    // This does not handle resizing windows docked to the main dockspace
    // Chances are some other edge cases not being handled as well, but it's good enough for ThreeL
    float scaleChange = scale / m_LastScale;
    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    for (int i = 0; i < m_Context->Windows.size(); i++)
    {
        ImGuiWindow* window = m_Context->Windows[i];

        if (window->ParentWindow != nullptr || window->Viewport != mainViewport)
        { continue; }

        ImVec2 pos = window->Pos;
        ImVec2 size = window->Size;
        pos.x -= mainViewport->Pos.x;
        pos.y -= mainViewport->Pos.y;

        ImGui::SetWindowPos(window, ImVec2(mainViewport->Pos.x + pos.x * scaleChange, mainViewport->Pos.y + pos.y * scaleChange));
        ImGui::SetWindowSize(window, ImVec2(size.x * scaleChange, size.y * scaleChange));
    }

    m_LastScale = scale;
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
std::optional<LRESULT> DearImGui::WndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    Assert(s_Singleton != nullptr);
    Assert(ImGui::GetCurrentContext() == s_Singleton->m_Context);
    LRESULT result = ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);
    return result == 0 ? std::nullopt : std::optional<LRESULT>(1);
}

DearImGui::~DearImGui()
{
    Assert(ImGui::GetCurrentContext() == m_Context);
    m_Window.RemoveWndProc(m_WndProcHandle);
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(m_Context);

    Assert(s_Singleton == this);
    s_Singleton = nullptr;
}

namespace ImGui
{
    bool Begin2(const char* name, bool* p_open, ImGuiWindowFlags flags)
    {
        if (p_open != nullptr && !*p_open)
        {
            ImGui::GetCurrentContext()->NextWindowData.ClearFlags();
            return false;
        }

        if (!ImGui::Begin(name, p_open, flags))
        {
            ImGui::End();
            return false;
        }

        return true;
    }

    void RightAlignedText(const std::string& s, float width)
    {
        ImVec2 size = ImGui::CalcTextSize(s);
        if (size.x < width)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (width - size.x));
        ImGui::Text(s);
    }
}
