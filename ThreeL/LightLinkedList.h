#pragma once
#include "pch.h"

#include "DynamicResourceDescriptor.h"
#include "RawGpuResource.h"
#include "ResourceDescriptor.h"
#include "ShaderInterop.h"
#include "Vector2.h"

class DepthStencilBuffer;
struct GraphicsContext;
class LightHeap;
struct ResourceManager;

class LightLinkedList
{
public:
    // Limited to 24 bits because upper 8 bits of index are the light index. See LightLink struct.
    // In a more refined implementation you'd likely limit this further anyway to avoid spending so much VRAM on it
    // (m_LightLinksHeap will end up being MAX_LIGHT_LINKS * sizeof(LightLink) which is currently 127 MB.)
    // It's unrestricted in ThreeL with a configurable articial limit for the sake of experimentation.
    static const uint32_t MAX_LIGHT_LINKS = 0xFFFFFF;

private:
    ResourceManager& m_Resources;

    D3D12_INDEX_BUFFER_VIEW m_LightSphereIndices;
    D3D12_VERTEX_BUFFER_VIEW m_LightSphereVertices;

    // RWByteAddressBuffer of ScreenW*ScreenH*4 bytes -- Each entry is the index of the first light link for that pixel
    // In a more realistic implementation you'd want to restrict this to the downsampled light buffer size you're using (IE: 1/8th in the original presentation.)
    // We allow configuring the light buffer size at runtime for the sake of experimentation, so we always allocate full-size
    RawGpuResource m_FirstLightLinkBuffer;
    D3D12_GPU_VIRTUAL_ADDRESS m_FirstLightLinkBufferGpuAddress;
    //TODO: Could remove these are they aren't actually being used anymore since I went with root descriptors for this buffer
    DynamicResourceDescriptor m_FirstLightLinkBufferUav;
    DynamicResourceDescriptor m_FirstLightLinkBufferSrv;

    // RWStructuredBuffer<LightLink> of MAX_LIGHT_LINKS entries
    // Each entry is a link in some pixel's light linked list (or garbage left over from a previous frame.)
    // The UAV counter marks the next free link in the heap. It's not totally clear if these separate counters are
    // actually worth using over atomic adds on modern hardware (once upon a time they were optimized by vendors.)
    RawGpuResource m_LightLinksHeap;
    ResourceDescriptor m_LightLinksHeapUav;
    ResourceDescriptor m_LightLinksHeapSrv;
    RawGpuResource m_LightLinksCounter;
    ResourceDescriptor m_LightLinksCounterUav;

public:
    //! The size specified is expected to be the full screen resolution, not the reduced resolution
    explicit LightLinkedList(ResourceManager& resources, uint2 initialSize);
    LightLinkedList(const LightLinkedList&) = delete;

    //! Resizes the per-pixel first link buffer
    //! The size specified is expected to be the full screen resolution, not the reduced resolution
    //! Caller asserts that this resource is no longer in use on the GPU
    void Resize(uint2 size);

    void FillLights
    (
        GraphicsContext& context,
        LightHeap& lightHeap,
        uint32_t lightCount,
        uint32_t lightLinkLimit,
        const ShaderInterop::PerFrameCb& perFrameCb,
        // Must match perFrameCb.LllBufferDivisor
        DepthStencilBuffer& depthBuffer,
        uint2 fullScreenSize
    );

    void DrawDebugOverlay(GraphicsContext& context, LightHeap& lightHeap, const ShaderInterop::PerFrameCb& perFrameCb);
};
