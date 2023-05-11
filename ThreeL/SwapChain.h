#pragma once
#include "pch.h"
#include "BackBuffer.h"
#include "GraphicsCore.h"
#include "Window.h"

#include <dxgi1_4.h>

class SwapChain
{
public:
    static const DXGI_FORMAT BACK_BUFFER_FORMAT = DXGI_FORMAT_B8G8R8A8_UNORM;
    static const uint32_t BACK_BUFFER_COUNT = 3;

private:
    GraphicsCore& m_GraphicsCore;
    ComPtr<IDXGISwapChain3> m_SwapChain;

    ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
    BackBuffer m_BackBuffers[BACK_BUFFER_COUNT];
    uint32_t m_CurrentBackBufferIndex;

public:
    SwapChain(GraphicsCore& graphicsCore, Window& window);

    inline BackBuffer& CurrentBackBuffer()
    {
        return m_BackBuffers[m_CurrentBackBufferIndex];
    }

    void Present();

    ~SwapChain();

    inline operator RenderTargetView()
    {
        return CurrentBackBuffer().GetRtv();
    }

private:
    void InitializeBackBuffers();
};
