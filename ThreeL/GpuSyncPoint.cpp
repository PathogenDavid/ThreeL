#include "pch.h"
#include "GpuSyncPoint.h"

void GpuSyncPoint::AssertReachedCold(ID3D12Fence* fence)
{
    if (fence->GetCompletedValue() >= m_FenceValue)
    {
        m_Fence = nullptr;
    }
    else
    {
        Assert(false && "The synchronization point has not been reached on the GPU.");
    }
}
