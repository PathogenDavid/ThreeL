#pragma once
#include "pch.h"
#include "GpuSyncPoint.h"

#include <mutex>
#include <queue>
#include <stack>

class CommandContext;
class GraphicsCore;

class CommandQueue
{
    //TODO: Too many friends here (a symptom of this class coming from C# and intending to use the internal access modifier)
    // Should maybe loosen things up or revisit some of the details behind how these types interact with eachother
    friend class CommandContext;
    friend struct ComputeContext;
    friend struct GraphicsContext;

    friend class UploadQueue;

    friend class SwapChain;

private:
    GraphicsCore& m_GraphicsCore;
    D3D12_COMMAND_LIST_TYPE m_Type;
    ComPtr<ID3D12CommandQueue> m_Queue;
    std::wstring m_Name;

    std::mutex m_FenceLock;
    uint64_t m_PreviousQueueFenceValue;
    ComPtr<ID3D12Fence> m_QueueFence;

    struct AvailableAllocator
    {
        uint64_t FenceValue;
        ID3D12CommandAllocator* Allocator;
    };

    std::mutex m_AvailableAllocatorsLock;
    std::queue<AvailableAllocator> m_AvailableAllocators;
    std::mutex m_AllAllocatorsLock;
    std::vector<ComPtr<ID3D12CommandAllocator>> m_AllAllocators;

    std::mutex m_AvailableContextsLock;
    std::stack<CommandContext*> m_AvailableContexts;
    std::mutex m_AllContextsLock;
    std::vector<std::unique_ptr<CommandContext>> m_AllContexts;
protected:
    CommandQueue(GraphicsCore& graphicsCore, D3D12_COMMAND_LIST_TYPE type);

private:
    ID3D12CommandAllocator* RentAllocator();

    GpuSyncPoint Execute(ID3D12CommandList* commandList);
    GpuSyncPoint ExecuteAndReturnAllocator(ID3D12CommandList* commandList, ID3D12CommandAllocator* allocator);

public:
    GpuSyncPoint QueueSyncPoint();

    inline void AwaitSyncPoint(GpuSyncPoint syncPoint)
    {
        if (syncPoint.m_Fence != nullptr)
        { m_Queue->Wait(syncPoint.m_Fence, syncPoint.m_FenceValue); }
    }

private:
    CommandContext& RentContext();
    void ReturnContext(CommandContext& context);

public:
    inline ID3D12CommandQueue* Queue() const { return m_Queue.Get(); }

    ~CommandQueue();
};

class GraphicsQueue : public CommandQueue
{
public:
    GraphicsQueue(GraphicsCore& graphicsCore)
        : CommandQueue(graphicsCore, D3D12_COMMAND_LIST_TYPE_DIRECT)
    {
    }
};

class ComputeQueue : public CommandQueue
{
public:
    ComputeQueue(GraphicsCore& graphicsCore)
        : CommandQueue(graphicsCore, D3D12_COMMAND_LIST_TYPE_COMPUTE)
    {
    }
};

// PIX compatibility
#include <pix3.h>
#ifdef USE_PIX
inline void PIXSetGPUMarkerOnContext(_In_ CommandQueue* context, _In_reads_bytes_(size) void* data, UINT size)
{
    PIXSetGPUMarkerOnContext(context->Queue(), data, size);
}

inline void PIXBeginGPUEventOnContext(_In_ CommandQueue* context, _In_reads_bytes_(size) void* data, UINT size)
{
    PIXBeginGPUEventOnContext(context->Queue(), data, size);
}

inline void PIXEndGPUEventOnContext(_In_ CommandQueue* context)
{
    PIXEndGPUEventOnContext(context->Queue());
}
#endif
