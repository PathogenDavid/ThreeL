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
    
    //TODO: This is inadequate for handling subresource state tracking or implicit resource state decay/promotion
    // This design also predates enhanced barriers and as such doesn't even slightly consider them
    // It should be revisited, don't imitate this.
    void TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES desiredState, bool immediate);

private:
    void UavBarrier(GpuResource* resource, bool immediate);
public:
    inline void UavBarrier(bool immediate) { UavBarrier(nullptr, immediate); }
    inline void UavBarrier(GpuResource& resource, bool immediate) { UavBarrier(&resource, immediate); }

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
    { }
#endif
};
