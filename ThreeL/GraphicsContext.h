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
        m_Context->FlushResourceBarriers();
        float color[] = { r, g, b, a };
        GetCommandList()->ClearRenderTargetView(renderTarget.RtvHandle(), color, 0, nullptr);
    }

    inline void Clear(const DepthStencilBuffer& depthBuffer)
    {
        m_Context->FlushResourceBarriers();
        GetCommandList()->ClearDepthStencilView(depthBuffer.View(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depthBuffer.DepthClearValue(), depthBuffer.StencilClearValue(), 0, nullptr);
    }

    inline void DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation = 0, uint32_t startInstanceLocation = 0)
    {
        m_Context->FlushResourceBarriers();
        GetCommandList()->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
    }

    inline void DrawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation = 0, uint32_t baseVertexLocation = 0, uint32_t startInstanceLocation = 0)
    {
        m_Context->FlushResourceBarriers();
        GetCommandList()->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
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

    inline void SetRenderTarget(DepthStencilView depthTarget)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthTarget.DsvHandle();
        GetCommandList()->OMSetRenderTargets(0, nullptr, false, &dsvHandle);
    }

    inline void SetFullViewportScissor(uint2 size)
    {
        D3D12_VIEWPORT viewport = { 0.f, 0.f, (float)size.x, (float)size.y, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
        GetCommandList()->RSSetViewports(1, &viewport);
        D3D12_RECT scissor = { 0, 0, (LONG)size.x, (LONG)size.y };
        GetCommandList()->RSSetScissorRects(1, &scissor);
    }

    inline GpuSyncPoint Flush(ID3D12PipelineState* newState = nullptr)
    {
        return m_Context->Flush(newState);
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
