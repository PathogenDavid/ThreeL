#include "pch.h"
#include "SamplerHeap.h"

#include "GraphicsCore.h"

SamplerHeap::SamplerHeap(GraphicsCore& graphics)
    : m_Graphics(graphics)
{
    D3D12_DESCRIPTOR_HEAP_DESC description =
    {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        .NumDescriptors = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 0,
    };
    AssertSuccess(graphics.Device()->CreateDescriptorHeap(&description, IID_PPV_ARGS(&m_DescriptorHeap)));
    m_DescriptorHeap->SetName(L"ARES Sampler Descriptor Heap");

    m_NextHandle = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    m_NextHandleIndex = 0;
    m_DescriptorSize = graphics.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
}

SamplerId SamplerHeap::Create(const D3D12_SAMPLER_DESC& sampler)
{
    // Return the existing sampler if one matching the description already exists
    auto cached = m_SamplerCache.find(sampler);
    if (cached != m_SamplerCache.end())
    {
        return cached->second;
    }

    // Create a fresh sampler and cache it
    Assert(m_NextHandleIndex < D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE && "Sampler heap exhausted");
    m_Graphics.Device()->CreateSampler(&sampler, m_NextHandle);

    uint32_t result = m_NextHandleIndex;
    m_SamplerCache.insert({ sampler, result });

    m_NextHandleIndex++;
    m_NextHandle.ptr += m_DescriptorSize;

    return result;
}
