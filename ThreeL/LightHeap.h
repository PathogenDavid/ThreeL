#pragma once
#include "FrequentlyUpdatedResource.h"
#include "GpuResource.h"
#include "GpuSyncPoint.h"
#include "Math.h"
#include "ShaderInterop.h"

#include <span>

class GraphicsCore;

class LightHeap : public GpuResource
{
public:
    // Light IDs are 8 bits so that's our hard limit
    static const uint32_t MAX_LIGHTS = 256;

private:
    FrequentlyUpdatedResource m_LightBuffer;

public:
    LightHeap(GraphicsCore& graphics);

    GpuSyncPoint Update(const std::span<const ShaderInterop::LightInfo>& lights);

    inline D3D12_GPU_VIRTUAL_ADDRESS BufferGpuAddress() const
    {
        return m_LightBuffer.Current()->GetGPUVirtualAddress();
    }
};
