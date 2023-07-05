#include "pch.h"
#include "SwapChain.h"

#include "CommandQueue.h"

SwapChain::SwapChain(GraphicsCore& graphicsCore, Window& window)
    : m_GraphicsCore(graphicsCore), m_Window(window)
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

    m_BufferSize = m_Size = { width, height };

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
        .Flags = SWAP_CHAIN_FLAGS,
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

    // Register WndProc to handle resizing
    m_WndProcHandle = window.AddWndProc([&](HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            if (message == WM_WINDOWPOSCHANGED)
            {
                RECT clientRect;
                Assert(GetClientRect(hwnd, &clientRect));
                uint2 newSize(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);

                if ((newSize > uint2::Zero).All()) // (Size can be 0 when window is minimized)
                {
                    Resize(newSize);
                }

                return std::optional<LRESULT>(0);
            }

            return std::optional<LRESULT>();
        });
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
        m_BackBuffers[i] = BackBuffer(renderTarget, rtvHandle);

        rtvHandle.ptr += rtvHandleSize;
    }

    m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
}

void SwapChain::Present()
{
    AssertSuccess(m_SwapChain->Present(0, 0));
    m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
}


void SwapChain::Resize(uint2 size)
{
    Assert((size.x > uint2::Zero).All() && "The swap chain size must be positive and non-zero!");

    // If there is no change, don't do anything
    if (m_Size == size)
    {
        return;
    }

    // All buffers should be presenting before resize
#ifdef DEBUG
    for (const BackBuffer& backBuffer : m_BackBuffers)
    {
        backBuffer.AssertIsInPresentState();
    }
#endif

    // If this is a minor size change, just change the source size instead of resizing the buffers
    //TODO: Add consideration of the VRAM limit here (IE: Never re-use the swap chain when the system is VRAM-constrained)
#if false //TODO: Disabled for now as we render directly to the swap chain and this is causing issues with the depth buffer size not matching
    if (size < m_BufferSize)
    {
        uint32_t bufferPixelCount = m_BufferSize.x * m_BufferSize.y;
        uint32_t newPixelCount = size.x * size.y;
        double percentDifference = (double)newPixelCount / (double)bufferPixelCount;

        // If the shrink doesn't exceed 75% of the buffer size, just resize the source region
        if (percentDifference > 0.75)
        {
            AssertSuccess(m_SwapChain->SetSourceSize(size.x, size.y));
            m_Size = size;
            return;
        }
    }
#endif

    // Wait for the graphics queue to become idle so we know nothing is in use
    m_GraphicsCore.GetGraphicsQueue().QueueSyncPoint().Wait();

    // Release the back buffers
    for (BackBuffer& backBuffer : m_BackBuffers)
    {
        backBuffer = { };
    }

    // Resize the buffers
    HRESULT hr = m_SwapChain->ResizeBuffers
    (
        BACK_BUFFER_COUNT,
        size.x,
        size.y,
        BACK_BUFFER_FORMAT,
        SWAP_CHAIN_FLAGS
    );
    AssertSuccess(hr);
    AssertSuccess(m_SwapChain->SetSourceSize(size.x, size.y));
    m_Size = m_BufferSize = size;

    // Re-initialize the render target views and such
    InitializeBackBuffers();
}

SwapChain::~SwapChain()
{
    m_Window.RemoveWndProc(m_WndProcHandle);

    // Wait for the graphics queue to become idle so we know nothing is in use
    m_GraphicsCore.GetGraphicsQueue().QueueSyncPoint().Wait();
}
