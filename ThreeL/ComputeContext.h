#pragma once
#include "CommandContext.h"
#include "GpuResource.h"
#include "GraphicsCore.h"
#include "ResourceDescriptor.h"
#include "Vector3.h"
#include "Vector4.h"

class ComputeQueue;
class GraphicsQueue;
class UavCounter;

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

    //TODO: This is to workaround our resource state tracking not working well with resources which map to multiple resourcs (IE: LightLinkedList or textures with subresources.)
    inline D3D12_RESOURCE_BARRIER& __AllocateResourceBarrier() { return m_Context->AllocateResourceBarrier(); }

    inline void ClearUav(ResourceDescriptor uavDescriptor, const GpuResource& resource, uint4 clearValue = uint4::Zero)
    {
        m_Context->FlushResourceBarriers();
        CommandList()->ClearUnorderedAccessViewUint(uavDescriptor.ResidentHandle(), uavDescriptor.StagingHandle(), resource.m_Resource.Get(), &clearValue.x, 0, nullptr);
    }

    void ClearUav(const UavCounter& counter, uint4 clearValue = uint4::Zero);

    inline void Dispatch(uint32_t x, uint32_t y = 1, uint32_t z = 1)
    {
        m_Context->FlushResourceBarriers();
        m_Context->m_CommandList->Dispatch(x, y, z);
    }

    inline void Dispatch(uint3 threadGroupCount) { Dispatch(threadGroupCount.x, threadGroupCount.y, threadGroupCount.z); }

    inline void DispatchIndirect(const GpuResource& argumentResource, uint32_t argumentOffset = 0)
    {
        m_Context->FlushResourceBarriers();
        CommandList()->ExecuteIndirect(m_Context->m_CommandQueue.m_GraphicsCore.DispatchIndirectCommandSignature(), 1, argumentResource.m_Resource.Get(), argumentOffset, nullptr, 0);
    }

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
