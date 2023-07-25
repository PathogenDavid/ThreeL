#include "pch.h"
#include "LightLinkedList.h"

#include "ComputeContext.h"
#include "DepthStencilBuffer.h"
#include "GraphicsContext.h"
#include "GraphicsCore.h"
#include "LightHeap.h"
#include "ResourceManager.h"

namespace
{
    extern const float LightSphereVertices[186];
    extern const uint16_t LightSphereIndices[360];
}

LightLinkedList::LightLinkedList(ResourceManager& resources, uint2 initialSize)
    : m_Resources(resources)
{
    GraphicsCore& graphics = m_Resources.Graphics;

    // Upload the light sphere mesh
    m_LightSphereIndices = m_Resources.MeshHeap.AllocateIndexBuffer(LightSphereIndices);
    m_LightSphereVertices = m_Resources.MeshHeap.AllocateVertexBuffer(LightSphereVertices, sizeof(LightSphereVertices), sizeof(float3));

    // Create light links heap and counter
    ComPtr<ID3D12Resource> lightLinksHeap;

    D3D12_HEAP_PROPERTIES heapProperties = { D3D12_HEAP_TYPE_DEFAULT };
    D3D12_RESOURCE_DESC resourceDescription = DescribeBufferResource(ShaderInterop::SizeOfLightLink * MAX_LIGHT_LINKS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    AssertSuccess(graphics.Device()->CreateCommittedResource
    (
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDescription,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&lightLinksHeap)
    ));
    lightLinksHeap->SetName(L"LightLinkedList LightLinksHeap");

    m_LightLinksCounter = UavCounter(graphics, L"LightLinkedList LightLinksHeap (Counter)");

    // Create resource views for light links heap
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDescription =
    {
        .Format = DXGI_FORMAT_UNKNOWN,
        .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
        .Buffer =
        {
            .FirstElement = 0,
            .NumElements = MAX_LIGHT_LINKS,
            .StructureByteStride = ShaderInterop::SizeOfLightLink,
            .CounterOffsetInBytes = 0,
            .Flags = D3D12_BUFFER_UAV_FLAG_NONE,
        },
    };
    m_LightLinksHeapUav = graphics.ResourceDescriptorManager().CreateUnorderedAccessView(lightLinksHeap.Get(), m_LightLinksCounter, uavDescription);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDescription =
    {
        .Format = DXGI_FORMAT_UNKNOWN,
        .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Buffer =
        {
            .FirstElement = 0,
            .NumElements = MAX_LIGHT_LINKS,
            .StructureByteStride = ShaderInterop::SizeOfLightLink,
            .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
        },
    };
    m_LightLinksHeapSrv = graphics.ResourceDescriptorManager().CreateShaderResourceView(lightLinksHeap.Get(), srvDescription);

    m_LightLinksHeap = RawGpuResource(std::move(lightLinksHeap));

    // Allocate the descriptors for the first light link buffer and "resize" to allocate the actual buffer
    m_FirstLightLinkBufferUav = graphics.ResourceDescriptorManager().AllocateDynamicDescriptor();
    m_FirstLightLinkBufferSrv = graphics.ResourceDescriptorManager().AllocateDynamicDescriptor();
    Resize(initialSize);
}

void LightLinkedList::Resize(uint2 size)
{
    ComPtr<ID3D12Resource> firstLightLink;

    // Allocate the first light link buffer
    D3D12_HEAP_PROPERTIES heapProperties = { D3D12_HEAP_TYPE_DEFAULT };
    D3D12_RESOURCE_DESC resourceDescription = DescribeBufferResource(size.x * size.y * sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    AssertSuccess(m_Resources.Graphics.Device()->CreateCommittedResource
    (
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDescription,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&firstLightLink)
    ));
    firstLightLink->SetName(L"LightLinkedList FirstLightLink Buffer");

    // Update the descriptors
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDescription =
    {
        .Format = DXGI_FORMAT_R32_TYPELESS,
        .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
        .Buffer =
        {
            .FirstElement = 0,
            .NumElements = (UINT)(resourceDescription.Width / sizeof(uint32_t)),
            .Flags = D3D12_BUFFER_UAV_FLAG_RAW,
        },
    };
    m_FirstLightLinkBufferUav.UpdateUnorderedAccessView(firstLightLink.Get(), nullptr, uavDescription);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDescription =
    {
        .Format = DXGI_FORMAT_R32_TYPELESS,
        .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Buffer =
        {
            .FirstElement = 0,
            .NumElements = (UINT)(resourceDescription.Width / sizeof(uint32_t)),
            .Flags = D3D12_BUFFER_SRV_FLAG_RAW,
        },
    };
    m_FirstLightLinkBufferSrv.UpdateShaderResourceView(firstLightLink.Get(), srvDescription);

    m_FirstLightLinkBuffer = RawGpuResource(std::move(firstLightLink));
}

void LightLinkedList::FillLights
(
    GraphicsContext& context,
    LightHeap& lightHeap,
    uint32_t lightCount,
    uint32_t lightLinkLimit,
    D3D12_GPU_VIRTUAL_ADDRESS perFrameCb,
    uint32_t lllBufferShift,
    DepthStencilBuffer& depthBuffer,
    uint2 fullScreenSize,
    const float4x4& perspectiveTransform
)
{
    uint2 lightLinkedListBufferSize = ScreenSizeToLllBufferSize(fullScreenSize, lllBufferShift);
    Assert((lightLinkedListBufferSize == depthBuffer.Size()).All() && "Light linked list buffer size and depth buffer size must match!");

    // Transition resources to their required states
    // (Depth buffer cannot be allowed to implicitly promote as it'll only implicitly promote to PIXEL_SHADER_RESOURCE or DEPTH_READ but not both.)
    context.TransitionResource(depthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    context.TransitionResource(m_FirstLightLinkBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    context.TransitionResource(m_LightLinksHeap, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    // Reset light buffers
    context.ClearUav(m_FirstLightLinkBufferUav, m_FirstLightLinkBuffer, uint4(0xFFFFFFFF));
    context.ClearUav(m_LightLinksCounter);

    // Draw active lights to fill the light linked list
    ShaderInterop::LightLinkedListFillParams params =
    {
        .LightLinksLimit = lightLinkLimit,
        .RangeExtensionRatio = 1.5f / ((float)lightLinkedListBufferSize.x * perspectiveTransform.m00),
    };

    context->SetGraphicsRootSignature(m_Resources.LightLinkedListFillRootSignature);
    context->SetPipelineState(m_Resources.LightLinkedListFill);
    context->SetGraphicsRoot32BitConstants(ShaderInterop::LightLinkedListFill::RpFillParams, sizeof(params) / sizeof(uint32_t), &params, 0);
    context->SetGraphicsRootConstantBufferView(ShaderInterop::LightLinkedListFill::RpPerFrameCb, perFrameCb);
    context->SetGraphicsRootShaderResourceView(ShaderInterop::LightLinkedListFill::RpLightHeap, lightHeap.BufferGpuAddress());
    context->SetGraphicsRootDescriptorTable(ShaderInterop::LightLinkedListFill::RpDepthBuffer, depthBuffer.DepthShaderResourceView().ResidentHandle());
    context->SetGraphicsRootDescriptorTable(ShaderInterop::LightLinkedListFill::RpLightLinksHeap, m_LightLinksHeapUav.ResidentHandle());
    context->SetGraphicsRootUnorderedAccessView(ShaderInterop::LightLinkedListFill::RpFirstLightLinkBuffer, m_FirstLightLinkBuffer.GpuAddress());
    context->IASetIndexBuffer(&m_LightSphereIndices);
    context->IASetVertexBuffers(MeshInputSlot::Position, 1, &m_LightSphereVertices);
    context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context.SetRenderTarget(depthBuffer.DepthReadOnlyView());
    context.SetFullViewportScissor(lightLinkedListBufferSize);
    context.DrawIndexedInstanced((uint32_t)std::size(LightSphereIndices), lightCount);

    // Flush UAV writes and transition resources to be read for lighting
    context.UavBarrier();
    context.TransitionResource(m_FirstLightLinkBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    context.TransitionResource(m_LightLinksHeap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void LightLinkedList::DrawDebugOverlay(GraphicsContext& context, LightHeap& lightHeap, D3D12_GPU_VIRTUAL_ADDRESS perFrameCb, const ShaderInterop::LightLinkedListDebugParams& params)
{
    using ShaderInterop::LightLinkedListDebugMode;
    Assert(params.Mode > LightLinkedListDebugMode::None && params.Mode <= LightLinkedListDebugMode::NearestLight && "Debug overlay mode must be valid!");
    context->SetGraphicsRootSignature(m_Resources.LightLinkedListDebugRootSignature);
    context->SetPipelineState(m_Resources.LightLinkedListDebug);
    context->SetGraphicsRoot32BitConstants(ShaderInterop::LightLinkedListDebug::RpDebugParams, sizeof(params) / sizeof(uint32_t), &params, 0);
    context->SetGraphicsRootConstantBufferView(ShaderInterop::LightLinkedListDebug::RpPerFrameCb, perFrameCb);
    context->SetGraphicsRootShaderResourceView(ShaderInterop::LightLinkedListDebug::RpLightHeap, lightHeap.BufferGpuAddress());
    context->SetGraphicsRootShaderResourceView(ShaderInterop::LightLinkedListDebug::RpLightLinksHeap, m_LightLinksHeap.GpuAddress());
    context->SetGraphicsRootShaderResourceView(ShaderInterop::LightLinkedListDebug::RpFirstLightLinkBuffer, m_FirstLightLinkBuffer.GpuAddress());
    context.DrawInstanced(3, 1);
}

void LightLinkedList::CollectStatistics(ComputeContext& context, uint2 fullScreenSize, uint32_t lllBufferShift, D3D12_GPU_VIRTUAL_ADDRESS resultsBuffer)
{
    uint2 lightLinkedListBufferSize = ScreenSizeToLllBufferSize(fullScreenSize, lllBufferShift);
    uint32_t lightLinkedListBufferLength = lightLinkedListBufferSize.x * lightLinkedListBufferSize.y;
    context->SetComputeRootSignature(m_Resources.LightLinkedListStatsRootSignature);
    context->SetPipelineState(m_Resources.LightLinkedListStats);
    context->SetComputeRoot32BitConstant(ShaderInterop::LightLinkedListStats::RpParams, lightLinkedListBufferLength, 0);
    context->SetComputeRootDescriptorTable(ShaderInterop::LightLinkedListStats::RpLightLinksHeapCounter, m_LightLinksCounter.Uav().ResidentHandle());
    context->SetComputeRootUnorderedAccessView(ShaderInterop::LightLinkedListStats::RpResults, resultsBuffer);
    context->SetComputeRootShaderResourceView(ShaderInterop::LightLinkedListStats::RpLightLinksHeap, m_LightLinksHeap.GpuAddress());
    context->SetComputeRootShaderResourceView(ShaderInterop::LightLinkedListStats::RpFirstLightLinkBuffer, m_FirstLightLinkBuffer.GpuAddress());
    context.Dispatch(Math::DivRoundUp(lightLinkedListBufferLength, ShaderInterop::LightLinkedListStats::ThreadGroupSize));
}

// This is just a UV sphere I made in Blender with 12 segments and 6 rings with a radius of 1.07
// The radius needs to be slightly above 1 so that the full light sphere is within this stand-in sphere
// Radius was determined experimentally. I solved it for 2D before I decided approximating in Blender against a high res sphere was good enough.
namespace
{
    const float LightSphereVertices[186] =
    {
        +0.267500f, +0.926647f, -0.463324f, +0.463324f, +0.535000f, -0.802500f, +0.535000f, +0.000000f, -0.926647f,
        +0.463324f, -0.535000f, -0.802500f, +0.267500f, -0.926647f, -0.463324f, +0.463324f, +0.926647f, -0.267500f,
        +0.802500f, +0.535000f, -0.463324f, +0.926647f, +0.000000f, -0.535000f, +0.802500f, -0.535000f, -0.463324f,
        +0.463324f, -0.926647f, -0.267500f, +0.535000f, +0.926647f, +0.000000f, +0.926647f, +0.535000f, +0.000000f,
        +1.070000f, +0.000000f, +0.000000f, +0.926647f, -0.535000f, +0.000000f, +0.535000f, -0.926647f, +0.000000f,
        +0.463324f, +0.926647f, +0.267500f, +0.802500f, +0.535000f, +0.463324f, +0.926647f, +0.000000f, +0.535000f,
        +0.802500f, -0.535000f, +0.463324f, +0.463324f, -0.926647f, +0.267500f, +0.267500f, +0.926647f, +0.463324f,
        +0.463324f, +0.535000f, +0.802500f, +0.535000f, +0.000000f, +0.926647f, +0.463324f, -0.535000f, +0.802500f,
        +0.267500f, -0.926647f, +0.463324f, +0.000000f, +0.926647f, +0.535000f, +0.000000f, +0.535000f, +0.926647f,
        +0.000000f, +0.000000f, +1.070000f, +0.000000f, -0.535000f, +0.926647f, +0.000000f, -0.926647f, +0.535000f,
        -0.267500f, +0.926647f, +0.463324f, -0.463324f, +0.535000f, +0.802500f, -0.535000f, +0.000000f, +0.926647f,
        -0.463324f, -0.535000f, +0.802500f, -0.267500f, -0.926647f, +0.463324f, -0.463324f, +0.926647f, +0.267500f,
        -0.802500f, +0.535000f, +0.463324f, -0.926647f, +0.000000f, +0.535000f, -0.802500f, -0.535000f, +0.463324f,
        -0.463324f, -0.926647f, +0.267500f, +0.000000f, -1.070000f, +0.000000f, -0.535000f, +0.926647f, +0.000000f,
        -0.926647f, +0.535000f, +0.000000f, -1.070000f, +0.000000f, +0.000000f, -0.926647f, -0.535000f, +0.000000f,
        -0.535000f, -0.926647f, +0.000000f, -0.463324f, +0.926647f, -0.267500f, -0.802500f, +0.535000f, -0.463324f,
        -0.926647f, +0.000000f, -0.535000f, -0.802500f, -0.535000f, -0.463324f, -0.463324f, -0.926647f, -0.267500f,
        -0.267500f, +0.926647f, -0.463324f, -0.463324f, +0.535000f, -0.802500f, -0.535000f, +0.000000f, -0.926647f,
        -0.463324f, -0.535000f, -0.802500f, -0.267500f, -0.926647f, -0.463324f, +0.000000f, +1.070000f, +0.000000f,
        +0.000000f, +0.926647f, -0.535000f, +0.000000f, +0.535000f, -0.926647f, +0.000000f, +0.000000f, -1.070000f,
        +0.000000f, -0.535000f, -0.926647f, +0.000000f, -0.926647f, -0.535000f,
    };

    const uint16_t LightSphereIndices[] =
    {
        4, 61, 40, 60,  3, 59, 58,  1, 57, 61,  4, 60, 59,  2, 58,  0, 56, 57,  1,  6,  0,  4,  9,  3,  2,  7,  1,  5, 56,  0,
        9,  4, 40,  3,  8,  2,  9, 14,  8,  7, 12,  6, 10, 56,  5, 14,  9, 40, 13, 12,  8, 11, 10,  6, 19, 18, 14, 12, 17, 11,
        15, 56, 10, 19, 14, 40, 13, 18, 12, 11, 16, 10, 19, 24, 18, 17, 22, 16, 20, 56, 15, 24, 19, 40, 18, 23, 17, 16, 21, 15,
        24, 29, 23, 22, 27, 21, 25, 56, 20, 29, 24, 40, 28, 27, 23, 21, 26, 20, 27, 32, 26, 30, 56, 25, 34, 29, 40, 28, 33, 27,
        26, 31, 25, 34, 33, 29, 35, 56, 30, 39, 34, 40, 33, 38, 32, 31, 36, 30, 34, 39, 33, 32, 37, 31, 45, 39, 40, 44, 43, 38,
        36, 42, 35, 39, 45, 38, 37, 43, 36, 41, 56, 35, 50, 45, 40, 44, 49, 43, 42, 47, 41, 50, 49, 45, 43, 48, 42, 46, 56, 41,
        55, 50, 40, 49, 54, 48, 47, 52, 46, 50, 55, 49, 48, 53, 47, 51, 56, 46, 60, 59, 54, 52, 58, 51, 55, 61, 54, 53, 59, 52,
        57, 56, 51, 61, 55, 40,  3,  2, 59,  1,  0, 57,  4,  3, 60,  2,  1, 58,  6,  5,  0,  9,  8,  3,  7,  6,  1,  8,  7,  2,
        14, 13,  8, 12, 11,  6, 12,  7,  8, 10,  5,  6, 18, 13, 14, 17, 16, 11, 18, 17, 12, 16, 15, 10, 24, 23, 18, 22, 21, 16,
        23, 22, 17, 21, 20, 15, 29, 28, 23, 27, 26, 21, 27, 22, 23, 26, 25, 20, 32, 31, 26, 33, 32, 27, 31, 30, 25, 33, 28, 29,
        38, 37, 32, 36, 35, 30, 39, 38, 33, 37, 36, 31, 43, 37, 38, 42, 41, 35, 45, 44, 38, 43, 42, 36, 49, 48, 43, 47, 46, 41,
        49, 44, 45, 48, 47, 42, 54, 53, 48, 52, 51, 46, 55, 54, 49, 53, 52, 47, 59, 53, 54, 58, 57, 51, 61, 60, 54, 59, 58, 52,
    };
}
