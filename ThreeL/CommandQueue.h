#pragma once
#include "GpuSyncPoint.h"
#include "Util.h"

#include <d3d12.h>
#include <memory>
#include <mutex>
#include <queue>
#include <stack>
#include <string>

class CommandContext;
class GraphicsCore;

class CommandQueue
{
    friend class CommandContext;
    friend struct ComputeContext;
    friend struct GraphicsContext;

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

    GpuSyncPoint QueueSyncPoint();

    CommandContext& RentContext();
    void ReturnContext(CommandContext& context);

public:
    inline const ComPtr<ID3D12CommandQueue>& GetQueue() const
    {
        return m_Queue;
    }

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
