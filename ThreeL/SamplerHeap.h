#pragma once
#include "pch.h"

class GraphicsCore;

using SamplerId = uint32_t;

class SamplerHeap
{
private:
    GraphicsCore& m_Graphics;
    ComPtr<ID3D12DescriptorHeap> m_DescriptorHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_NextHandle;
    uint32_t m_NextHandleIndex;
    UINT m_DescriptorSize;
    std::unordered_map<D3D12_SAMPLER_DESC, uint32_t> m_SamplerCache;

public:
    SamplerHeap(GraphicsCore& graphics);

    SamplerId Create(const D3D12_SAMPLER_DESC& sampler);

    inline ID3D12DescriptorHeap* GetGpuHeap()
    {
        return m_DescriptorHeap.Get();
    }
};
