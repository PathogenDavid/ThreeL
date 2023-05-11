#pragma once
#include "BackBuffer.h"
#include "GraphicsCore.h"
#include "Util.h"
#include "Window.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <Windows.h>

class SwapChain
{
private:
    static const DXGI_FORMAT BACK_BUFFER_FORMAT = DXGI_FORMAT_B8G8R8A8_UNORM;
    static const uint32_t BACK_BUFFER_COUNT = 3;

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

private:
    void InitializeBackBuffers();
};
