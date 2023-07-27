#pragma once
#include "pch.h"
#include "BackBuffer.h"
#include "GraphicsCore.h"
#include "Math.h"
#include "Window.h"

#include <dxgi1_4.h>

enum class PresentMode
{
    Vsync,
    NoSync,
    Tearing,
};

class SwapChain
{
public:
    // My laptop misbehaves when DXGI_FORMAT_R16G16B16A16_FLOAT
    // Documentation on using this format for swapchains is scant, so it's unclear if this is due to
    // something we're doing wrong or something maybe Intel's drivers are doing wrong.
    // Regardless, we weren't really taking advantage of the increased precision anyway.
    // In the future I might implement proper HDR support (either for rendering or presenting), but first I probably need to acquire a decent HDR monitor.
    //static const DXGI_FORMAT BACK_BUFFER_FORMAT = DXGI_FORMAT_R16G16B16A16_FLOAT;
    static const DXGI_FORMAT BACK_BUFFER_FORMAT = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    static const uint32_t BACK_BUFFER_COUNT = 3;

private:
    GraphicsCore& m_GraphicsCore;
    Window& m_Window;
    ComPtr<IDXGISwapChain3> m_SwapChain;
    DXGI_SWAP_CHAIN_FLAG m_SwapChainFlags;
    bool m_SupportsTearing;
    DXGI_FORMAT m_UnderlyingFormat;

    ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
    BackBuffer m_BackBuffers[BACK_BUFFER_COUNT];
    GpuSyncPoint m_SyncPoints[BACK_BUFFER_COUNT];
    uint32_t m_CurrentBackBufferIndex;

    uint2 m_Size;
    uint2 m_BufferSize;

    WndProcHandle m_WndProcHandle;

public:
    SwapChain(GraphicsCore& graphicsCore, Window& window);

    inline BackBuffer& CurrentBackBuffer() { return m_BackBuffers[m_CurrentBackBufferIndex]; }
    inline const BackBuffer& CurrentBackBuffer() const { return m_BackBuffers[m_CurrentBackBufferIndex]; }

    void Present(PresentMode mode);

    ~SwapChain();

    inline operator RenderTargetView() const { return CurrentBackBuffer().Rtv(); }
    inline uint2 Size() const { return m_Size; }
    inline bool SupportsTearing() const { return m_SupportsTearing; }

private:
    void InitializeBackBuffers();
    void Resize(uint2 size);
};
