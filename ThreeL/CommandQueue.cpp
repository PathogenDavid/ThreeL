#include "pch.h"
#include "CommandQueue.h"

#include "CommandContext.h"
#include "GraphicsCore.h"

CommandQueue::CommandQueue(GraphicsCore& graphicsCore, D3D12_COMMAND_LIST_TYPE type)
    : m_GraphicsCore(graphicsCore), m_Type(type)
{
    D3D12_COMMAND_QUEUE_DESC description =
    {
        .Type = type,
        .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
        .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
        .NodeMask = 0,
    };
    AssertSuccess(graphicsCore.GetDevice()->CreateCommandQueue(&description, IID_PPV_ARGS(&m_Queue)));
    m_Name = std::format(L"ARES CommandQueue({})", (uint32_t)type);
    m_Queue->SetName(m_Name.c_str());

    m_PreviousQueueFenceValue = 0;
    AssertSuccess(graphicsCore.GetDevice()->CreateFence(m_PreviousQueueFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_QueueFence)));
    m_QueueFence->SetName(std::format(L"{} Fence", m_Name).c_str());
}

ID3D12CommandAllocator* CommandQueue::RentAllocator()
{
    ID3D12CommandAllocator* result = nullptr;

    // Try to get an existing allocator that is no longer in use by the GPU
    {
        std::lock_guard<std::mutex> lock(m_AvailableAllocatorsLock);
        if (!m_AvailableAllocators.empty())
        {
            AvailableAllocator allocator = m_AvailableAllocators.front();
            if (allocator.FenceValue <= m_QueueFence->GetCompletedValue())
            {
                result = allocator.Allocator;
                m_AvailableAllocators.pop();
            }
        }
    }

    // Reset the existing allocator or create a new one
    if (result != nullptr)
    {
        AssertSuccess(result->Reset());
    }
    else
    {
        ComPtr<ID3D12CommandAllocator> newAllocator;
        AssertSuccess(m_GraphicsCore.GetDevice()->CreateCommandAllocator(m_Type, IID_PPV_ARGS(&newAllocator)));
        result = newAllocator.Get();

        std::lock_guard<std::mutex> lock(m_AllAllocatorsLock);
        newAllocator->SetName(std::format(L"{} Command Allocator #{}", m_Name, m_AllAllocators.size() + 1).c_str());
        m_AllAllocators.push_back(newAllocator);
    }

    return result;
}

GpuSyncPoint CommandQueue::Execute(ID3D12CommandList* commandList)
{
    std::lock_guard<std::mutex> lock(m_FenceLock);

    m_Queue->ExecuteCommandLists(1, &commandList);

    m_PreviousQueueFenceValue++;
    uint64_t fenceValue = m_PreviousQueueFenceValue;
    AssertSuccess(m_Queue->Signal(m_QueueFence.Get(), fenceValue));

    return { m_QueueFence.Get(), fenceValue };
}

GpuSyncPoint CommandQueue::ExecuteAndReturnAllocator(ID3D12CommandList* commandList, ID3D12CommandAllocator* allocator)
{
    // Execute the command list and create a final sync point so we know when the allocator is able to be reset
    GpuSyncPoint syncPoint = Execute(commandList);

    // Enqueue the allocator so it can be later reused
    {
        std::lock_guard<std::mutex> lock(m_AvailableAllocatorsLock);
        m_AvailableAllocators.push({ syncPoint.m_FenceValue, allocator });
    }

    return syncPoint;
}

GpuSyncPoint CommandQueue::QueueSyncPoint()
{
    std::lock_guard<std::mutex> lock(m_FenceLock);

    m_PreviousQueueFenceValue++;
    uint64_t fenceValue = m_PreviousQueueFenceValue;
    AssertSuccess(m_Queue->Signal(m_QueueFence.Get(), fenceValue));

    return { m_QueueFence.Get(), fenceValue };
}

CommandContext& CommandQueue::RentContext()
{
    CommandContext* result = nullptr;

    // Try to get an existing context that is no logner in use
    {
        std::lock_guard<std::mutex> lock(m_AvailableContextsLock);
        if (!m_AvailableContexts.empty())
        {
            result = m_AvailableContexts.top();
            m_AvailableContexts.pop();
        }
    }

    // If none was available, create a new one
    if (result == nullptr)
    {
        result = new CommandContext(*this);
        {
            std::lock_guard<std::mutex> lock(m_AllContextsLock);
            std::unique_ptr<CommandContext> resultPtr(result);
            m_AllContexts.push_back(std::move(resultPtr));
        }
    }

    return *result;
}

void CommandQueue::ReturnContext(CommandContext& context)
{
    std::lock_guard<std::mutex> lock(m_AvailableContextsLock);
    m_AvailableContexts.push(&context);
}

CommandQueue::~CommandQueue()
{
    // Wait for the GPU to finish using all allocators (and by extension the queue its self)
    // (Don't reuse the PreviousQueueFenceValue signal here since it doesn't account for implicitly added work like swap chain presents.)
    constexpr uint64_t fenceValue = std::numeric_limits<uint64_t>::max();
    AssertSuccess(m_Queue->Signal(m_QueueFence.Get(), fenceValue));
    AssertSuccess(m_QueueFence->SetEventOnCompletion(fenceValue, nullptr));
}
