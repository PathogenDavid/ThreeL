#include "pch.h"
#include "DearImGui.h"

#include "GraphicsContext.h"
#include "GraphicsCore.h"
#include "SwapChain.h"
#include "Window.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

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
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.IniFilename = nullptr;

    //---------------------------------------------------------------------------------------------------------
    // Configure Style
    //---------------------------------------------------------------------------------------------------------
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();

    // Disable window rounding and window background alpha since it doesn't play nice with multiple viewports
    style.WindowRounding = 0.f;
    style.Colors[ImGuiCol_WindowBg].w = 1.f;

    //---------------------------------------------------------------------------------------------------------
    // Initialize platform/renderer backends
    //---------------------------------------------------------------------------------------------------------
    ImGui_ImplWin32_Init(window.GetHwnd());
    auto fontDescriptor = graphicsCore.GetResourceDescriptorManager().AllocateUninitializedResidentDescriptor();
    ImGui_ImplDX12_Init
    (
        graphicsCore.GetDevice().Get(),
        SwapChain::BACK_BUFFER_COUNT,
        SwapChain::BACK_BUFFER_FORMAT,
        graphicsCore.GetResourceDescriptorManager().GetGpuHeap(),
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
}

void DearImGui::Render(GraphicsContext& context)
{
    Assert(ImGui::GetCurrentContext() == m_Context);
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), context.GetCommandList());
}

void DearImGui::RenderViewportWindows()
{
    if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault(nullptr, nullptr);
    }
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
