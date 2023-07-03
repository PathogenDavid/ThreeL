#pragma once
#include "pch.h"
#include "GpuSyncPoint.h"

class CommandQueue;
class GpuResource;
class SwapChain;

class CommandContext final
{
    friend struct GraphicsContext;
    friend struct ComputeContext;
    friend class UploadQueue;

private:
    CommandQueue& m_CommandQueue;
    ID3D12CommandAllocator* m_CommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_CommandList;

    D3D12_RESOURCE_BARRIER m_PendingResourceBarriers[16];
    uint32_t m_PendingResourceBarrierCount = 0;

public:
    CommandContext(CommandQueue& commandQueue);
    CommandContext(const CommandContext&) = delete;

    void Begin(ID3D12PipelineState* initialPipelineState);
    
    void TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES desiredState, bool immediate);

    void FlushResourceBarriers();
    GpuSyncPoint Flush(ID3D12PipelineState* newState);

    GpuSyncPoint Finish();

private:
    void ResetCommandList(ID3D12PipelineState* initialPipelineState);
    D3D12_RESOURCE_BARRIER& AllocateResourceBarrier();
    void FlushResourceBarriersEarly();

#ifdef DEBUG
    std::vector<GpuResource*> m_OwnedResources;
    void TakeResource(GpuResource& resource);
#else
    inline void TakeResource(GpuResource& resource)
    {
    }
#endif
};
