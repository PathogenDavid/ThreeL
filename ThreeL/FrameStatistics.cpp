#include "pch.h"
#include "FrameStatistics.h"

#include "GraphicsContext.h"
#include "GraphicsCore.h"
#include "LightLinkedList.h"

FrameStatistics::FrameStatistics(GraphicsCore& graphics)
    : m_Graphics(graphics)
{
    // Create buffer for collecting statistics from compute shaders
    {
        ComPtr<ID3D12Resource> computeBuffer;
        D3D12_HEAP_PROPERTIES heapProperties = { D3D12_HEAP_TYPE_DEFAULT };
        D3D12_RESOURCE_DESC resourceDescription = DescribeBufferResource(sizeof(StatisticsBuffer), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        AssertSuccess(graphics.Device()->CreateCommittedResource
        (
            &heapProperties,
            D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
            &resourceDescription,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&computeBuffer)
        ));
        computeBuffer->SetName(L"Statistics Compute Buffer");

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDescription =
        {
            .Format = DXGI_FORMAT_R32_TYPELESS,
            .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
            .Buffer =
            {
                .FirstElement = 0,
                .NumElements = (UINT)(resourceDescription.Width / sizeof(uint32_t)),
                .Flags = D3D12_BUFFER_UAV_FLAG_RAW,
            },
        };
        m_StatisticsComputeBufferUav = graphics.ResourceDescriptorManager().CreateUnorderedAccessView(computeBuffer.Get(), nullptr, uavDescription);

        m_StatisticsComputeBufferAddress = computeBuffer->GetGPUVirtualAddress();
        m_StatisticsComputeBuffer = RawGpuResource(std::move(computeBuffer));
    }

    // Create buffer for reading statistics from CPU
    {
        D3D12_HEAP_PROPERTIES heapProperties = { D3D12_HEAP_TYPE_READBACK };
        D3D12_RESOURCE_DESC resourceDescription = DescribeBufferResource(sizeof(StatisticsBuffer) * BUFFER_COUNT, D3D12_RESOURCE_FLAG_NONE);
        AssertSuccess(graphics.Device()->CreateCommittedResource
        (
            &heapProperties,
            D3D12_HEAP_FLAG_NONE, // We rely on this heap being zeroed by the runtime
            &resourceDescription,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_StatisticsReadbackBuffer)
        ));
        m_StatisticsReadbackBuffer->SetName(L"Statistics Readback Buffer");
    }

    // Pretend to start a frame so that m_CurrentStatistics is mapped to something (all statistics will be 0)
    m_NextWriteBuffer = 0;
    m_CurrentReadBuffer = BUFFER_COUNT - 2;
    AdvanceReadBuffer();
}

void FrameStatistics::AdvanceReadBuffer()
{
    uint32_t nextReadBuffer = m_CurrentReadBuffer;
    for (uint32_t i = 1; i < BUFFER_COUNT; i++)
    {
        uint32_t maybeNextRead = (m_CurrentReadBuffer + i) % BUFFER_COUNT;

        // Don't try to read a buffer from the future
        if (maybeNextRead == m_NextWriteBuffer)
        { break; }

        // If this buffer isn't finished on the GPU, we stop looking
        if (!m_SyncPoints[maybeNextRead].WasReached())
        { break; }

        // This buffer is valid, it might be our next read buffer as long as there isn't a more recent buffer available
        nextReadBuffer = maybeNextRead;
    }

    // If the read buffer didn't change, there's nothing to do
    if (nextReadBuffer == m_CurrentReadBuffer)
    { return; }

    // Unmap the old statistics buffer and map the new one
    if (m_CurrentStatistics != nullptr)
    {
        D3D12_RANGE emptyRange = { };
        m_StatisticsReadbackBuffer->Unmap(0, &emptyRange);
        m_CurrentStatistics = nullptr;
    }

    m_CurrentReadBuffer = nextReadBuffer;
    D3D12_RANGE readRange =
    {
        .Begin = sizeof(StatisticsBuffer) * m_CurrentReadBuffer,
        .End = sizeof(StatisticsBuffer) * (m_CurrentReadBuffer + 1),
    };
    AssertSuccess(m_StatisticsReadbackBuffer->Map(0, &readRange, (void**)&m_CurrentStatistics));
}

void FrameStatistics::StartFrame()
{
    AdvanceReadBuffer();
}

void FrameStatistics::StartCollectStatistics(GraphicsContext& context)
{
    context.TransitionResource(m_StatisticsComputeBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
    context.ClearUav(m_StatisticsComputeBufferUav, m_StatisticsComputeBuffer);
}

void FrameStatistics::FinishCollectStatistics(GraphicsContext& context)
{
    m_SyncPoints[m_NextWriteBuffer].AssertReached(); // Ensure the write target is not in use on the GPU

    // If we want to write to the current read buffer, that probably just means the read buffer is falling behind
    if (m_NextWriteBuffer == m_CurrentReadBuffer)
    {
        AdvanceReadBuffer();
        Assert(m_NextWriteBuffer != m_CurrentReadBuffer && "Statistics collection is going to try to write to the buffer currently mapped for reading!");
    }

    context.UavBarrier(m_StatisticsComputeBuffer);
    context.TransitionResource(m_StatisticsComputeBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE, true);
    context->CopyBufferRegion(m_StatisticsReadbackBuffer.Get(), sizeof(StatisticsBuffer) * m_NextWriteBuffer, m_StatisticsComputeBuffer, 0, sizeof(StatisticsBuffer));
    m_SyncPoints[m_NextWriteBuffer] = context.Flush();

    m_NextWriteBuffer = (m_NextWriteBuffer + 1) % BUFFER_COUNT;
}

FrameStatistics::~FrameStatistics()
{
    if (m_CurrentStatistics != nullptr)
    {
        D3D12_RANGE emptyRange = { };
        m_StatisticsReadbackBuffer->Unmap(0, &emptyRange);
        m_CurrentStatistics = nullptr;
    }
}
