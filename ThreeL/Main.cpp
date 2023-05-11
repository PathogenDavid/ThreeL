#include "pch.h"
#include "BackBuffer.h"
#include "CommandQueue.h"
#include "DearImGui.h"
#include "DebugLayer.h"
#include "GraphicsContext.h"
#include "GraphicsCore.h"
#include "SwapChain.h"
#include "Window.h"

#include "imgui.h"
#include <pix3.h>

static int MainImpl()
{
    GraphicsCore graphics;

    Window window(L"ThreeL", 1280, 720);

    // Don't show the PIX HUD on Dear ImGui viewports
    PIXSetTargetWindow(window.GetHwnd());
    PIXSetHUDOptions(PIX_HUD_SHOW_ON_TARGET_WINDOW_ONLY);

    SwapChain swapChain(graphics, window);

    DearImGui dearImGui(graphics, window);

    window.Show();

    bool pingPong = true;

    while (Window::ProcessMessages())
    {
        dearImGui.NewFrame();

        ImGui::ShowDemoWindow();

        GraphicsContext context(graphics.GetGraphicsQueue());
        context.TransitionResource(swapChain, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
        context.Clear(swapChain, pingPong ? 1.f : 0.f, 0.f, pingPong ? 0.f : 1.f, 1.f);
        context.SetRenderTarget(swapChain);
        context.TransitionResource(swapChain, D3D12_RESOURCE_STATE_PRESENT);

        dearImGui.Render(context);

        context.Finish();

        dearImGui.RenderViewportWindows();

        swapChain.Present();

        pingPong = !pingPong;
    }

    return 0;
}

int main()
{
    int result = MainImpl();
    DebugLayer::ReportLiveObjects();
    return result;
}
