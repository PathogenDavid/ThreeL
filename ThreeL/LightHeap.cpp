#include "pch.h"
#include "LightHeap.h"

#include "GraphicsCore.h"

LightHeap::LightHeap(GraphicsCore& graphics)
    : m_LightBuffer(graphics, DescribeBufferResource(LightHeap::MAX_LIGHTS * sizeof(ShaderInterop::LightInfo)), L"Light Info Heap")
{
}

GpuSyncPoint LightHeap::Update(const std::span<const ShaderInterop::LightInfo>& lights)
{
    Assert(lights.size() < MAX_LIGHTS && "Light count exceeds heap capacity!");
    return m_LightBuffer.Update(lights);
}
