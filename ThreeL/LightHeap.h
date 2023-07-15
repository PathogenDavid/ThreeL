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
    // Limited to 8 bits because lower 24 bits of index are the next light link index. See LightLink struct.
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
