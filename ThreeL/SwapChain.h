#pragma once
#include "pch.h"
#include "BackBuffer.h"
#include "GraphicsCore.h"
#include "Math.h"
#include "Window.h"

#include <dxgi1_4.h>

class SwapChain
{
public:
    static const DXGI_FORMAT BACK_BUFFER_FORMAT = DXGI_FORMAT_R16G16B16A16_FLOAT;
    static const uint32_t BACK_BUFFER_COUNT = 3;
    static const DXGI_SWAP_CHAIN_FLAG SWAP_CHAIN_FLAGS = (DXGI_SWAP_CHAIN_FLAG)0;

private:
    GraphicsCore& m_GraphicsCore;
    Window& m_Window;
    ComPtr<IDXGISwapChain3> m_SwapChain;

    ComPtr<ID3D12DescriptorHeap> m_RtvHeap;
    BackBuffer m_BackBuffers[BACK_BUFFER_COUNT];
    uint32_t m_CurrentBackBufferIndex;

    uint2 m_Size;
    uint2 m_BufferSize;

    WndProcHandle m_WndProcHandle;

public:
    SwapChain(GraphicsCore& graphicsCore, Window& window);

    inline BackBuffer& CurrentBackBuffer() { return m_BackBuffers[m_CurrentBackBufferIndex]; }
    inline const BackBuffer& CurrentBackBuffer() const { return m_BackBuffers[m_CurrentBackBufferIndex]; }

    void Present();

    ~SwapChain();

    inline operator RenderTargetView() const { return CurrentBackBuffer().GetRtv(); }
    inline uint2 Size() const { return m_Size; }

private:
    void InitializeBackBuffers();
    void Resize(uint2 size);
};
