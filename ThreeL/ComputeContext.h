#pragma once
#include "CommandContext.h"
#include "Vector3.h"

class ComputeQueue;
class GraphicsQueue;

struct ComputeContext
{
private:
    CommandContext* m_Context;

    ComputeContext(CommandQueue& queue);
    ComputeContext(CommandQueue& queue, ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState);

public:
    ComputeContext(ComputeQueue& queue) : ComputeContext((CommandQueue&)queue) { }
    ComputeContext(GraphicsQueue& queue) : ComputeContext((CommandQueue&)queue) { }
    ComputeContext(ComputeQueue& queue, ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState) : ComputeContext((CommandQueue&)queue, rootSignature, pipelineState) { }
    ComputeContext(GraphicsQueue& queue, ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState) : ComputeContext((CommandQueue&)queue, rootSignature, pipelineState) { }

    ComputeContext(const ComputeContext&) = delete;

    inline ID3D12GraphicsCommandList* CommandList() const { return m_Context->m_CommandList.Get(); }
    inline ID3D12GraphicsCommandList* operator->() const { return CommandList(); }

    inline void TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES desiredState, bool immediate = false)
    {
        m_Context->TransitionResource(resource, desiredState, immediate);
    }

    inline void UavBarrier(bool immediate = false) { m_Context->UavBarrier(immediate); }
    inline void UavBarrier(GpuResource& resource, bool immediate = false) { m_Context->UavBarrier(resource, immediate); }

    //TODO: This is to work around our resource state tracking being worefully inadequate for tracking the state of individual subresources
    inline D3D12_RESOURCE_BARRIER& __AllocateResourceBarrier() { return m_Context->AllocateResourceBarrier(); }

    void Dispatch(uint3 threadGroupCount);

    inline GpuSyncPoint Flush(ID3D12PipelineState* newState)
    {
        m_Context->Flush(newState);
    }

    GpuSyncPoint Finish();

    ~ComputeContext()
    {
        if (m_Context != nullptr)
        {
            Assert(false && "A ComputeContext was allowed to go out of scope before being submitted!");
            Finish();
        }
    }
};

// PIX compatibility
#include <pix3.h>
#ifdef USE_PIX
inline void PIXSetGPUMarkerOnContext(_In_ ComputeContext* context, _In_reads_bytes_(size) void* data, UINT size)
{
    PIXSetGPUMarkerOnContext(context->CommandList(), data, size);
}

inline void PIXBeginGPUEventOnContext(_In_ ComputeContext* context, _In_reads_bytes_(size) void* data, UINT size)
{
    PIXBeginGPUEventOnContext(context->CommandList(), data, size);
}

inline void PIXEndGPUEventOnContext(_In_ ComputeContext* context)
{
    PIXEndGPUEventOnContext(context->CommandList());
}
#endif
