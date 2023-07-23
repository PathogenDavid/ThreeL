#pragma once
#include "pch.h"

#include "RawGpuResource.h"
#include "SwapChain.h"

struct GraphicsContext;
class GraphicsCore;

class FrameStatistics
{
private:
    // We have one additional statistics buffer so that the GPU is able to be writing to all three and still have the current buffer be valid
    static const uint32_t BUFFER_COUNT = SwapChain::BACK_BUFFER_COUNT + 1;

    GraphicsCore& m_Graphics;

    RawGpuResource m_StatisticsComputeBuffer;
    ResourceDescriptor m_StatisticsComputeBufferUav;

    ComPtr<ID3D12Resource> m_StatisticsReadbackBuffer;

    GpuSyncPoint m_SyncPoints[BUFFER_COUNT];

    struct StatisticsBuffer
    {
        uint32_t NumberOfLightLinksUsed;
        uint32_t MaximumLightCountForAnyPixel;
    };

    StatisticsBuffer* m_CurrentStatistics = nullptr;
    uint32_t m_NextWriteBuffer;
    uint32_t m_CurrentReadBuffer;

public:
    FrameStatistics(GraphicsCore& graphics);

private:
    void AdvanceReadBuffer();
public:
    void StartFrame();

    void StartCollectStatistics(GraphicsContext& context);
    void FinishCollectStatistics(GraphicsContext& context);

    inline D3D12_GPU_VIRTUAL_ADDRESS LightLinkedListStatisticsLocation() const { return m_StatisticsComputeBuffer.GpuAddress(); }

    inline uint32_t NumberOfLightLinksUsed() const { return m_CurrentStatistics->NumberOfLightLinksUsed; }
    inline uint32_t MaximumLightCountForAnyPixel() const { return m_CurrentStatistics->MaximumLightCountForAnyPixel; }

    ~FrameStatistics();
};
