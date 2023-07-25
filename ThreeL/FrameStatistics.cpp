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
        computeBuffer->SetName(L"FrameStatistics Compute Buffer");

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
        m_StatisticsReadbackBuffer->SetName(L"FrameStatistics Readback Buffer");
    }

    // Create the query heap for collecting GPU timestamps
    {
        D3D12_QUERY_HEAP_DESC queryHeapDescription =
        {
            .Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP,
            .Count = (uint32_t)Timer::COUNT * 2 * BUFFER_COUNT,
        };
        AssertSuccess(graphics.Device()->CreateQueryHeap(&queryHeapDescription, IID_PPV_ARGS(&m_QueryHeap)));
        m_QueryHeap->SetName(L"FrameStatistics GPU Timestamp Query Heap");

        // Ensure all entires in the query heap are initialized so that we don't need to worry about undefined behavior should we read a timer without setting it
        ComputeContext context(graphics.GraphicsQueue());
        for (uint32_t i = 0; i < queryHeapDescription.Count; i++)
        {
            context->EndQuery(m_QueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, i);
        }
        context.Finish();
    }

    // Get frequency of CPU and GPU timers
    {
        LARGE_INTEGER cpuTimestampFrequency;
        Assert(QueryPerformanceFrequency(&cpuTimestampFrequency));
        m_CpuTimestampFrequency = (double)cpuTimestampFrequency.QuadPart;

        UINT64 graphicsTimestampFrequency;
        AssertSuccess(graphics.GraphicsQueue().Queue()->GetTimestampFrequency(&graphicsTimestampFrequency));
        m_GpuTimestampFrequency = (double)graphicsTimestampFrequency;

        // It's expected that graphics and async compute queues always have the same frequency
        // If they weren't, we'd want to keep track of which queue was used for each timer and apply the correct frequency
        // The reason GetTimestampFrequency is on ID3D12CommandQueue is video *can* be different.
        // https://discord.com/channels/590611987420020747/590965902564917258/796768164327063584
        UINT64 computeTimestampFrequency;
        AssertSuccess(graphics.ComputeQueue().Queue()->GetTimestampFrequency(&computeTimestampFrequency));
        Assert(graphicsTimestampFrequency == computeTimestampFrequency && "Graphics and compute timestamp frequencies are expected to match!");
    }

    // Clear the CPU timestamps
    LARGE_INTEGER now;
    Assert(QueryPerformanceCounter(&now));
    for (size_t i = 0; i < std::size(m_CpuTimestampsBuffer); i++)
    { m_CpuTimestampsBuffer[i] = 0 * now.QuadPart; }

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

    // Copy the CPU timestamps for the buffer we just read to the publicly-accessible buffer
    // (We can't just "map" a segment of m_CpuTimeStampsBuffer since it could get clobbered when the GPU is running behind but the CPU isn't.)
    memcpy(m_CurrentCpuTimestamps, m_CpuTimestampsBuffer + (TIMER_BLOCK_COUNT * m_CurrentReadBuffer), sizeof(m_CurrentCpuTimestamps));
}

void FrameStatistics::StartFrame()
{
    AdvanceReadBuffer();
}

void FrameStatistics::CaptureTimestamp(ID3D12GraphicsCommandList* commandList, Timer timer, bool isEnd)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    // Determine the timer index for the current write buffer
    // Note that unlike FinishCollectStatistics, it's OK for us to write even when m_NextReadBuffer == m_CurrentReadBuffer
    // since the main thing being protected there is the mapped statistics buffer, which we are not writing to.
    uint32_t timerIndex = (m_NextWriteBuffer * TIMER_BLOCK_COUNT) + ((uint32_t)timer * 2 + (isEnd ? 1 : 0));

    m_CpuTimestampsBuffer[timerIndex] = now.QuadPart;
    commandList->EndQuery(m_QueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, timerIndex);
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

    context->ResolveQueryData
    (
        m_QueryHeap.Get(),
        D3D12_QUERY_TYPE_TIMESTAMP,
        m_NextWriteBuffer * TIMER_BLOCK_COUNT,
        TIMER_BLOCK_COUNT,
        m_StatisticsReadbackBuffer.Get(),
        offsetof(StatisticsBuffer, GpuTimestamps)
    );

    m_SyncPoints[m_NextWriteBuffer] = context.Flush();

    m_NextWriteBuffer = (m_NextWriteBuffer + 1) % BUFFER_COUNT;
}

double FrameStatistics::GetElapsedTime(uint64_t* timestamps, Timer timer, double frequency)
{
    uint64_t start = timestamps[(uint32_t)timer * 2];
    uint64_t end = timestamps[(uint32_t)timer * 2 + 1];

    // End should not come before start
    if (end < start)
    { return TIMER_INVALID; }

    // If the end is before the start of the frame, the timer was not tracked for that frame
    if (timer != Timer::FrameTotal && end < timestamps[(uint32_t)Timer::FrameTotal])
    { return TIMER_SKIPPED; }

    uint64_t delta = end - start;
    return (double)delta / frequency;
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
