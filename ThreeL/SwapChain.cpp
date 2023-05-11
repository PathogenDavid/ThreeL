#include "pch.h"
#include "SwapChain.h"

#include "CommandQueue.h"

SwapChain::SwapChain(GraphicsCore& graphicsCore, Window& window)
    : m_GraphicsCore(graphicsCore)
{
    // Determine initial size
    uint32_t width;
    uint32_t height;
    {
        RECT clientRect;
        Assert(GetClientRect(window.GetHwnd(), &clientRect));
        width = clientRect.right - clientRect.left;
        height = clientRect.bottom - clientRect.top;
    }

    // Create the swap chain
    DXGI_SWAP_CHAIN_DESC1 desc =
    {
        .Width = width,
        .Height = height,
        .Format = BACK_BUFFER_FORMAT,
        .SampleDesc = { 1, 0 },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = BACK_BUFFER_COUNT,
        .Scaling = DXGI_SCALING_STRETCH,
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
        .AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
        .Flags = 0,
    };

    ComPtr<IDXGISwapChain1> swapChain;
    HRESULT hr = graphicsCore.GetDxgiFactory()->CreateSwapChainForHwnd(
        graphicsCore.GetGraphicsQueue().GetQueue().Get(),
        window.GetHwnd(),
        &desc,
        nullptr,
        nullptr,
        &swapChain
    );
    AssertSuccess(hr);
    AssertSuccess(swapChain.As(&this->m_SwapChain));

    // Create render target view descriptor heap and corresponding views
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDescription =
    {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        .NumDescriptors = BACK_BUFFER_COUNT,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    };
    AssertSuccess(graphicsCore.GetDevice()->CreateDescriptorHeap(&rtvHeapDescription, IID_PPV_ARGS(&m_RtvHeap)));
    m_RtvHeap->SetName(L"ARES SwapChain RTV heap");
    InitializeBackBuffers();
}

void SwapChain::InitializeBackBuffers()
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RtvHeap->GetCPUDescriptorHandleForHeapStart();
    uint32_t rtvHandleSize = m_GraphicsCore.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    for (uint32_t i = 0; i < std::size(m_BackBuffers); i++)
    {
        ComPtr<ID3D12Resource> renderTarget;
        AssertSuccess(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&renderTarget)));
        renderTarget->SetName(std::format(L"ARES SwapChain back buffer {}", i).c_str());
        m_GraphicsCore.GetDevice()->CreateRenderTargetView(renderTarget.Get(), nullptr, rtvHandle);
        m_BackBuffers[i] = { renderTarget, rtvHandle };

        rtvHandle.ptr += rtvHandleSize;
    }

    m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
}

void SwapChain::Present()
{
    AssertSuccess(m_SwapChain->Present(0, 0));
    m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
}

SwapChain::~SwapChain()
{
    // Wait for the graphics queue to become idle so we know nothing is in use
    m_GraphicsCore.GetGraphicsQueue().QueueSyncPoint().Wait();
}
