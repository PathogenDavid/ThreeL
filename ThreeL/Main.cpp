#include "Assert.h"
#include "BackBuffer.h"
#include "CommandQueue.h"
#include "DebugLayer.h"
#include "GraphicsContext.h"
#include "GraphicsCore.h"
#include "SwapChain.h"
#include "Window.h"

#include <stdio.h>

static int MainImpl()
{
    GraphicsCore graphics;

    Window window(L"ThreeL", 1280, 720);
    window.Show();

    SwapChain swapChain(graphics, window);

    bool pingPong = true;

    while (Window::ProcessMessages())
    {
        GraphicsContext context(graphics.GetGraphicsQueue());
        context.TransitionResource(swapChain, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
        context.Clear(swapChain.CurrentBackBuffer().GetRtv(), pingPong ? 1.f : 0.f, 0.f, pingPong ? 0.f : 1.f, 1.f);
        context.TransitionResource(swapChain, D3D12_RESOURCE_STATE_PRESENT);
        context.Finish();

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
