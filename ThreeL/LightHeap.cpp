#include "pch.h"
#include "LightHeap.h"

#include "GraphicsCore.h"

static D3D12_RESOURCE_DESC resourceDescription =
{
    .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
    .Alignment = 0,
    .Width = LightHeap::MAX_LIGHTS * sizeof(ShaderInterop::LightInfo),
    .Height = 1,
    .DepthOrArraySize = 1,
    .MipLevels = 1,
    .Format = DXGI_FORMAT_UNKNOWN,
    .SampleDesc = { .Count = 1, .Quality = 0 },
    .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    .Flags = D3D12_RESOURCE_FLAG_NONE,
};

LightHeap::LightHeap(GraphicsCore& graphics)
    : m_LightBuffer(graphics, resourceDescription, L"Light Info Heap")
{
}

GpuSyncPoint LightHeap::Update(const std::span<const ShaderInterop::LightInfo>& lights)
{
    Assert(lights.size() < MAX_LIGHTS && "Light count exceeds heap capacity!");
    return m_LightBuffer.Update(lights);
}
