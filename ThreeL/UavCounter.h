#pragma once
#include "GpuResource.h"
#include "ResourceDescriptor.h"

class GraphicsCore;

//PERF: It might be worth investigating if we should just recommend co-locating the buffer and its counter or should have a dedicated UAV counter heap that gets split up into multiple counters.
// (On paper we're technically wasting the majority of a whole 64K buffer for a single uint due to alignment rules -- whereas counters in general only need alignment of 4K.)
// In D3D11 UAV counters were hidden because they had special handling on the hardware (basically they lived in small, globally-accessible/fast chunk of memory.)
// In D3D12 they're no longer hidden, but it's not clear if drivers might still be capable of applying the D3D11-era optimization.
//   And if they are, it's further unclear if doing anything "clever" with the counter might defeat said optimization.
class UavCounter : public GpuResource
{
private:
    ResourceDescriptor m_Uav;
    D3D12_GPU_VIRTUAL_ADDRESS m_GpuAddress = 0;

public:
    UavCounter() = default;

    UavCounter(GraphicsCore& graphics, const std::wstring& debugName);

    inline ID3D12Resource* Resource() const { return m_Resource.Get(); }
    inline operator ID3D12Resource* () const { return Resource(); }

    inline ResourceDescriptor Uav() const { return m_Uav; };
    inline D3D12_GPU_VIRTUAL_ADDRESS GpuAddress() const { return m_GpuAddress; };
};
