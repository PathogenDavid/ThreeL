#pragma once
#include "pch.h"
#include "DynamicDescriptorTableBuilder.h"
#include "DynamicResourceDescriptor.h"
#include "ResourceDescriptor.h"

#include <tuple>

class GraphicsCore;

/// <summary>Provides facilities for allocating CBV/SRV/UAV descriptors.</summary>
/// <remarks>
/// A single instance of this class manages all CBV/SRV/UAV descriptors for a <see cref="GraphicsCore"/>.
/// We only permit a single instance because switching between descriptor heaps is very expensive on some hardware.
///
/// The GPU heap is divided into two segments:
/// * The resident descriptor segment
/// * The dynamic descriptor segment
///
/// The resident descriptor segment is used for the individual descriptors corresponding to long-lived resources like textures.
///
/// The dynamic descriptor segment is a ring buffer of ephemeral descriptors used to build descriptor tables during rendering.
///
/// (In the future we might add the ability to pre-allocate long-lived descriptor tables out of the resident segment, but this is currently not supported.)
///
/// Reisdent descriptors are initially initialized on a CPU-only staging heap and copied to the same location in the resident descriptor segment for use on the GPU.
/// </remarks>
class ResourceDescriptorManager
{
    friend struct DynamicDescriptorTableBuilder;
    friend struct DynamicResourceDescriptor;

private:
    GraphicsCore& m_Graphics;
    uint32_t m_DescriptorSize;

    ComPtr<ID3D12DescriptorHeap> m_StagingHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_StagingHeapHandle0;

    ComPtr<ID3D12DescriptorHeap> m_GpuHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_ResidentCpuHandle0;
    D3D12_GPU_DESCRIPTOR_HANDLE m_ResidentGpuHandle0;
    D3D12_CPU_DESCRIPTOR_HANDLE m_DynamicCpuHandle0;
    D3D12_GPU_DESCRIPTOR_HANDLE m_DynamicGpuHandle0;

    uint32_t m_NextFreeResidentDescriptorIndex = 0;
    uint64_t m_NextFreeDynamicDescriptorIndex = 0;

    static const uint32_t RESIDENT_DESCRIPTOR_COUNT = 4096;
#ifdef DEBUG
    // On debug builds, use a more modestly-sized descriptor heap so we more easily notice out of control heap usage
    static const uint32_t TOTAL_DESCRIPTOR_COUNT = RESIDENT_DESCRIPTOR_COUNT * 2;
#else
    // For release builds just allocate the maximum-size tier 1 descriptor heap
    // https://docs.microsoft.com/en-us/windows/win32/direct3d12/hardware-support
    static const uint32_t TOTAL_DESCRIPTOR_COUNT = 1'000'000;
#endif
    static const uint32_t DYNAMIC_DESCRIPTOR_COUNT = TOTAL_DESCRIPTOR_COUNT - RESIDENT_DESCRIPTOR_COUNT;

public:
    ResourceDescriptorManager(GraphicsCore& graphics);

private:
    uint32_t AllocateResidentDescriptor();

public:
    DynamicResourceDescriptor AllocateDynamicDescriptor();

    ResourceDescriptor CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC& description);
    ResourceDescriptor CreateShaderResourceView(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& description);
    ResourceDescriptor CreateUnorderedAccessView(ID3D12Resource* resource, ID3D12Resource* counterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& description);

    std::tuple<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> AllocateUninitializedResidentDescriptor();

    DynamicDescriptorTableBuilder AllocateDynamicTable(uint32_t length);

    inline ID3D12DescriptorHeap* GpuHeap() const { return m_GpuHeap.Get(); }

    //TODO: This exists to enable bindless, ideally we should probably just rip out all the bindful stuff or make it the exception instead
    inline uint32_t GetResidentIndex(const ResourceDescriptor& descriptor)
    {
        return (uint32_t)((descriptor.ResidentHandle().ptr - m_ResidentGpuHandle0.ptr) / m_DescriptorSize);
    }
};
