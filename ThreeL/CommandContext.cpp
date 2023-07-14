#include "pch.h"
#include "CommandContext.h"

#include "CommandQueue.h"
#include "GpuResource.h"
#include "GraphicsCore.h"

static volatile uint64_t g_NextId;

CommandContext::CommandContext(CommandQueue& commandQueue)
    : m_CommandQueue(commandQueue)
{
    ID3D12Device* device = commandQueue.m_GraphicsCore.Device();
    memset(m_PendingResourceBarriers, 0, sizeof(m_PendingResourceBarriers));

    // When Device4 is available, use it to create the command list in a closed state.
    // Otherwise simulate it by creating a command list and immediately closing it.
    // (We do not provide a path where it is created open because ideally creation should be relatively rare since contexts are pooled.)
    ComPtr<ID3D12Device4> device4;
    if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&device4))))
    {
        m_CommandAllocator = nullptr;
        AssertSuccess(device4->CreateCommandList1(0, commandQueue.m_Type, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&m_CommandList)));
    }
    else
    {
        m_CommandAllocator = commandQueue.RentAllocator();
        AssertSuccess(device->CreateCommandList(0, commandQueue.m_Type, m_CommandAllocator, nullptr, IID_PPV_ARGS(&m_CommandList)));
        Finish();
    }

    m_CommandList->SetName(std::format(L"ARES CommandContext({}) #{}", (uint32_t)commandQueue.m_Type, InterlockedIncrement(&g_NextId)).c_str());
}

void CommandContext::Begin(ID3D12PipelineState* initialPipelineState)
{
    Assert(m_CommandAllocator == nullptr && "Tried to begin an already-active context!");
    m_CommandAllocator = m_CommandQueue.RentAllocator();
    ResetCommandList(initialPipelineState);
}

void CommandContext::ResetCommandList(ID3D12PipelineState* initialPipelineState)
{
    Assert(m_CommandAllocator != nullptr && "Tried to reset without an allocator.");
    m_CommandList->Reset(m_CommandAllocator, initialPipelineState);
    ID3D12DescriptorHeap* heaps[] =
    {
        m_CommandQueue.m_GraphicsCore.ResourceDescriptorManager().GpuHeap(),
        m_CommandQueue.m_GraphicsCore.SamplerHeap().GpuHeap(),
    };

    if (m_CommandQueue.m_Type != D3D12_COMMAND_LIST_TYPE_COPY)
    { m_CommandList->SetDescriptorHeaps((UINT)std::size(heaps), heaps); }

    if (m_CommandQueue.m_Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
    { m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); }
}

D3D12_RESOURCE_BARRIER& CommandContext::AllocateResourceBarrier()
{
    // If we're out of pending resource barriers, flush them now
    if (m_PendingResourceBarrierCount == std::size(m_PendingResourceBarriers))
        FlushResourceBarriersEarly();
    
    D3D12_RESOURCE_BARRIER& result = m_PendingResourceBarriers[m_PendingResourceBarrierCount];
    m_PendingResourceBarrierCount++;
    return result;
}

void CommandContext::TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES desiredState, bool immediate)
{
    // Always take the resource even if we don't actually do the transition for the sake of consistency
    TakeResource(resource);

    D3D12_RESOURCE_STATES currentState = resource.m_CurrentState;

    // Ignore no-op transitions
    if (currentState == desiredState)
    {
        if (immediate)
            FlushResourceBarriersEarly();

        return;
    }

    AllocateResourceBarrier() =
    {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition =
        {
            .pResource = resource.m_Resource.Get(),
            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            .StateBefore = currentState,
            .StateAfter = desiredState,
        },
    };
    resource.m_CurrentState = desiredState;

    if (immediate)
        FlushResourceBarriersEarly();
}

void CommandContext::UavBarrier(GpuResource* resource, bool immediate)
{
    AllocateResourceBarrier() =
    {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_UAV,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .UAV =
        {
            .pResource = resource == nullptr ? nullptr : resource->m_Resource.Get()
        }
    };

    if (immediate)
        FlushResourceBarriersEarly();
}

void CommandContext::FlushResourceBarriersEarly()
{
    if (m_PendingResourceBarrierCount == 0)
        return;

    m_CommandList->ResourceBarrier(m_PendingResourceBarrierCount, m_PendingResourceBarriers);
    m_PendingResourceBarrierCount = 0;
}

void CommandContext::FlushResourceBarriers()
{
    FlushResourceBarriersEarly();

#if DEBUG
    if (!m_OwnedResources.empty())
    {
        for (GpuResource* resource : m_OwnedResources)
        {
            resource->ReleaseOwnership(this);
        }

        m_OwnedResources.clear();
    }
#endif
}

GpuSyncPoint CommandContext::Flush(ID3D12PipelineState* newState)
{
    Assert(m_CommandAllocator != nullptr && "Tried to flush an inactive context!");

    FlushResourceBarriers();

    AssertSuccess(m_CommandList->Close());
    GpuSyncPoint syncPoint = m_CommandQueue.Execute(m_CommandList.Get());
    ResetCommandList(newState);

    return syncPoint;
}

GpuSyncPoint CommandContext::Finish()
{
    Assert(m_CommandAllocator != nullptr && "Tried to finish an inactive context!");

    FlushResourceBarriers();

    AssertSuccess(m_CommandList->Close());
    GpuSyncPoint syncPoint = m_CommandQueue.ExecuteAndReturnAllocator(m_CommandList.Get(), m_CommandAllocator);
    m_CommandAllocator = nullptr;

    return syncPoint;
}

#ifdef DEBUG
void CommandContext::TakeResource(GpuResource& resource)
{
    resource.TakeOwnership(this);
    m_OwnedResources.push_back(&resource);
}
#endif
