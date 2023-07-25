#pragma once
#include "pch.h"

#include "ComputeContext.h"
#include "GraphicsContext.h"
#include "RawGpuResource.h"
#include "SwapChain.h"

class GraphicsCore;

enum class Timer
{
    FrameTotal,
    FrameSetup,
    ParticleUpdate,
    DepthPrePass,
    DownsampleDepth,
    FillLightLinkedList,
    OpaquePass,
    ParticleRender,
    DebugOverlay,
    UI,
    Present,
    COUNT,
};

class FrameStatistics
{
private:
    // We have one additional statistics buffer so that the GPU is able to be writing to all three and still have the current buffer be valid
    static const uint32_t BUFFER_COUNT = SwapChain::BACK_BUFFER_COUNT + 1;
    static const uint32_t TIMER_BLOCK_COUNT = (int)Timer::COUNT * 2;

    GraphicsCore& m_Graphics;

    RawGpuResource m_StatisticsComputeBuffer;
    ResourceDescriptor m_StatisticsComputeBufferUav;

    ComPtr<ID3D12QueryHeap> m_QueryHeap;
    
    double m_CpuTimestampFrequency;
    double m_GpuTimestampFrequency;

    // CPU timerstamps are also buffered like GPU statistics just to ensure they stay in sync with eachother
    // (IE: That way the time spent on the GPU and CPU are from the same frame. Otherwise GPU stats would have latency while CPU would not.)
    uint64_t m_CpuTimestampsBuffer[TIMER_BLOCK_COUNT * BUFFER_COUNT];

    ComPtr<ID3D12Resource> m_StatisticsReadbackBuffer;

    GpuSyncPoint m_SyncPoints[BUFFER_COUNT];

    struct StatisticsBuffer
    {
        uint32_t NumberOfLightLinksUsed;
        uint32_t MaximumLightCountForAnyPixel;
        uint64_t GpuTimestamps[TIMER_BLOCK_COUNT];
    };
    static_assert(offsetof(StatisticsBuffer, GpuTimestamps) % sizeof(uint64_t) == 0);

    StatisticsBuffer* m_CurrentStatistics = nullptr;
    uint64_t m_CurrentCpuTimestamps[TIMER_BLOCK_COUNT];
    uint32_t m_NextWriteBuffer;
    uint32_t m_CurrentReadBuffer;

public:
    FrameStatistics(GraphicsCore& graphics);

private:
    void AdvanceReadBuffer();

public:
    void StartFrame();

private:
    void CaptureTimestamp(ID3D12GraphicsCommandList* commandList, Timer timer, bool isEnd);

public:
    inline void StartTimer(GraphicsContext& context, Timer timer) { CaptureTimestamp(context.CommandList(), timer, false); }
    inline void EndTimer(GraphicsContext& context, Timer timer) { CaptureTimestamp(context.CommandList(), timer, true); }
    inline void StartTimer(ComputeContext& context, Timer timer) { CaptureTimestamp(context.CommandList(), timer, false); }
    inline void EndTimer(ComputeContext& context, Timer timer) { CaptureTimestamp(context.CommandList(), timer, true); }

    struct ScopedTimer
    {
        friend class FrameStatistics;
    private:
        FrameStatistics& m_FrameStatistics;
        ID3D12GraphicsCommandList* m_CommandList;
        Timer m_Timer;

        ScopedTimer(FrameStatistics& frameStatistics, ID3D12GraphicsCommandList* commandList, Timer timer)
            : m_FrameStatistics(frameStatistics), m_CommandList(commandList), m_Timer(timer)
        {
            m_FrameStatistics.CaptureTimestamp(m_CommandList, m_Timer, false);
        }

        ScopedTimer(const ScopedTimer&) = delete;
    public:
        ~ScopedTimer() { m_FrameStatistics.CaptureTimestamp(m_CommandList, m_Timer, true); }
    };

    ScopedTimer MakeScopedTimer(GraphicsContext& context, Timer timer) { return ScopedTimer(*this, context.CommandList(), timer); }
    ScopedTimer MakeScopedTimer(ComputeContext& context, Timer timer) { return ScopedTimer(*this, context.CommandList(), timer); }

    void StartCollectStatistics(GraphicsContext& context);
    void FinishCollectStatistics(GraphicsContext& context);

    inline D3D12_GPU_VIRTUAL_ADDRESS LightLinkedListStatisticsLocation() const { return m_StatisticsComputeBuffer.GpuAddress(); }

    inline uint32_t NumberOfLightLinksUsed() const { return m_CurrentStatistics->NumberOfLightLinksUsed; }
    inline uint32_t MaximumLightCountForAnyPixel() const { return m_CurrentStatistics->MaximumLightCountForAnyPixel; }

    // Special return values for ElapsedTimeCpu/ElapsedTimeGpu
    static inline double TIMER_SKIPPED = -1.0; // Indicates that the timer was not captured during this frame
    static inline double TIMER_INVALID = -2.0; // An invalid timer means the end of the timer came before the start

private:
    double GetElapsedTime(uint64_t* timestamps, Timer timer, double frequency);
public:
    inline double GetElapsedTimeCpu(Timer timer) { return GetElapsedTime(m_CurrentCpuTimestamps, timer, m_CpuTimestampFrequency); }
    inline double GetElapsedTimeGpu(Timer timer) { return GetElapsedTime(m_CurrentStatistics->GpuTimestamps, timer, m_GpuTimestampFrequency); }

    ~FrameStatistics();
};
