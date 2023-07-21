#pragma once
#include "CommandContext.h"
#include "DepthStencilBuffer.h"
#include "GpuResource.h"
#include "RenderTargetView.h"
#include "SwapChain.h"
#include "Vector4.h"

class GraphicsQueue;

struct GraphicsContext
{
private:
    CommandContext* m_Context;

public:
    GraphicsContext(GraphicsQueue& queue);
    GraphicsContext(GraphicsQueue& queue, ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState);
    GraphicsContext(const GraphicsContext&) = delete;

    inline ID3D12GraphicsCommandList* CommandList() const { return m_Context->m_CommandList.Get(); }
    inline ID3D12GraphicsCommandList* operator->() const { return CommandList(); }

    inline void TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES desiredState, bool immediate = false)
    {
        m_Context->TransitionResource(resource, desiredState, immediate);
    }

    inline void TransitionResource(SwapChain& swapChain, D3D12_RESOURCE_STATES desiredState, bool immediate = false)
    {
        m_Context->TransitionResource(swapChain.CurrentBackBuffer(), desiredState, immediate);
    }

    inline void UavBarrier(bool immediate = false) { m_Context->UavBarrier(immediate); }
    inline void UavBarrier(GpuResource& resource, bool immediate = false) { m_Context->UavBarrier(resource, immediate); }

    //TODO: This is to workaround our resource state tracking not working well with resources which map to multiple resourcs (IE: LightLinkedList or textures with subresources.)
    inline D3D12_RESOURCE_BARRIER& __AllocateResourceBarrier() { return m_Context->AllocateResourceBarrier(); }

    inline void Clear(RenderTargetView renderTarget, float r, float g, float b, float a)
    {
        m_Context->FlushResourceBarriers();
        float color[] = { r, g, b, a };
        CommandList()->ClearRenderTargetView(renderTarget.RtvHandle(), color, 0, nullptr);
    }

    inline void Clear(const DepthStencilBuffer& depthBuffer)
    {
        m_Context->FlushResourceBarriers();
        CommandList()->ClearDepthStencilView(depthBuffer.View(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, depthBuffer.DepthClearValue(), depthBuffer.StencilClearValue(), 0, nullptr);
    }

    inline void ClearUav(ResourceDescriptor uavDescriptor, const GpuResource& resource, uint4 clearValue = uint4::Zero)
    {
        m_Context->FlushResourceBarriers();
        CommandList()->ClearUnorderedAccessViewUint(uavDescriptor.ResidentHandle(), uavDescriptor.StagingHandle(), resource.m_Resource.Get(), &clearValue.x, 0, nullptr);
    }

    inline void DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount, uint32_t startVertexLocation = 0, uint32_t startInstanceLocation = 0)
    {
        m_Context->FlushResourceBarriers();
        CommandList()->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
    }

    inline void DrawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation = 0, uint32_t baseVertexLocation = 0, uint32_t startInstanceLocation = 0)
    {
        m_Context->FlushResourceBarriers();
        CommandList()->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
    }

    //TODO: Actual intent is that there'd be a cast from GraphicsContext to ComputeContext, but the adjacent infrastrucutre making that worth it is missing so didn't bother.
    inline void Dispatch(uint3 threadGroupCount)
    {
        m_Context->FlushResourceBarriers();
        m_Context->m_CommandList->Dispatch(threadGroupCount.x, threadGroupCount.y, threadGroupCount.z);
    }

    inline void SetRenderTarget(RenderTargetView renderTarget)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = renderTarget.RtvHandle();
        CommandList()->OMSetRenderTargets(1, &rtvHandle, false, nullptr);
    }

    inline void SetRenderTarget(RenderTargetView renderTarget, DepthStencilView depthTarget)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = renderTarget.RtvHandle();
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthTarget.DsvHandle();
        CommandList()->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);
    }

    inline void SetRenderTarget(DepthStencilView depthTarget)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = depthTarget.DsvHandle();
        CommandList()->OMSetRenderTargets(0, nullptr, false, &dsvHandle);
    }

    inline void SetFullViewportScissor(uint2 size)
    {
        D3D12_VIEWPORT viewport = { 0.f, 0.f, (float)size.x, (float)size.y, D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
        CommandList()->RSSetViewports(1, &viewport);
        D3D12_RECT scissor = { 0, 0, (LONG)size.x, (LONG)size.y };
        CommandList()->RSSetScissorRects(1, &scissor);
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
    PIXSetGPUMarkerOnContext(context->CommandList(), data, size);
}

inline void PIXBeginGPUEventOnContext(_In_ GraphicsContext* context, _In_reads_bytes_(size) void* data, UINT size)
{
    PIXBeginGPUEventOnContext(context->CommandList(), data, size);
}

inline void PIXEndGPUEventOnContext(_In_ GraphicsContext* context)
{
    PIXEndGPUEventOnContext(context->CommandList());
}
#endif
