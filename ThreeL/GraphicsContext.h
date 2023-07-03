#pragma once
#include "CommandContext.h"
#include "DepthStencilBuffer.h"
#include "RenderTargetView.h"
#include "SwapChain.h"

class GraphicsQueue;

struct GraphicsContext
{
private:
    CommandContext* m_Context;

public:
    GraphicsContext(GraphicsQueue& queue);
    GraphicsContext(GraphicsQueue& queue, ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState);
    GraphicsContext(const GraphicsContext&) = delete;

    inline ID3D12GraphicsCommandList* GetCommandList() const
    {
        return m_Context->m_CommandList.Get();
    }

    inline ID3D12GraphicsCommandList* operator->() const
    {
        return GetCommandList();
    }

    inline void TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES desiredState, bool immediate = false)
    {
        m_Context->TransitionResource(resource, desiredState, immediate);
    }

    inline void TransitionResource(SwapChain& swapChain, D3D12_RESOURCE_STATES desiredState, bool immediate = false)
    {
        m_Context->TransitionResource(swapChain.CurrentBackBuffer(), desiredState, immediate);
    }

    inline void Clear(RenderTargetView renderTarget, float r, float g, float b, float a)
    {
        float color[] = { r, g, b, a };
        GetCommandList()->ClearRenderTargetView(renderTarget.RtvHandle(), color, 0, nullptr);
    }

    inline void Clear(const DepthStencilBuffer& depthBuffer)
    {
        GetCommandList()->ClearDepthStencilView(depthBuffer.View(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depthBuffer.DepthClearValue(), depthBuffer.StencilClearValue(), 0, nullptr);
    }

    inline void SetRenderTarget(RenderTargetView renderTarget)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = renderTarget.RtvHandle();
        GetCommandList()->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
    }

    inline void SetRenderTarget(RenderTargetView renderTarget, DepthStencilView depthTarget)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = renderTarget.RtvHandle();
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthTarget.DsvHandle();
        GetCommandList()->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
    }

    inline GpuSyncPoint Flush(ID3D12PipelineState* newState)
    {
        m_Context->Flush(newState);
    }

    GpuSyncPoint Finish();

    ~GraphicsContext()
    {
        if (m_Context != nullptr)
        {
            Assert(false && "A GraphicsContext was allowed to go out of scope before being submitted!");
            Finish();
        }
    }
};

// PIX compatibility
#include <pix3.h>
#ifdef USE_PIX
inline void PIXSetGPUMarkerOnContext(_In_ GraphicsContext* context, _In_reads_bytes_(size) void* data, UINT size)
{
    PIXSetGPUMarkerOnContext(context->GetCommandList(), data, size);
}

inline void PIXBeginGPUEventOnContext(_In_ GraphicsContext* context, _In_reads_bytes_(size) void* data, UINT size)
{
    PIXBeginGPUEventOnContext(context->GetCommandList(), data, size);
}

inline void PIXEndGPUEventOnContext(_In_ GraphicsContext* context)
{
    PIXEndGPUEventOnContext(context->GetCommandList());
}
#endif
