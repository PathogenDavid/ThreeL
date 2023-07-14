#include "pch.h"
#include "ResourceDescriptorManager.h"

#include "GraphicsCore.h"

ResourceDescriptorManager::ResourceDescriptorManager(GraphicsCore& graphics)
    : m_Graphics(graphics)
{
    m_DescriptorSize = m_Graphics.Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_DESCRIPTOR_HEAP_DESC heapDescription =
    {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = RESIDENT_DESCRIPTOR_COUNT,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    };

    AssertSuccess(m_Graphics.Device()->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&m_StagingHeap)));
    m_StagingHeap->SetName(L"ARES CBV/SRV/UAV Descriptor Staging Heap");
    m_StagingHeapHandle0 = m_StagingHeap->GetCPUDescriptorHandleForHeapStart();

    heapDescription.NumDescriptors = TOTAL_DESCRIPTOR_COUNT;
    heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    AssertSuccess(m_Graphics.Device()->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&m_GpuHeap)));
    m_GpuHeap->SetName(L"ARES CBV/SRV/UAV Descriptor GPU Heap");
    m_ResidentCpuHandle0 = m_GpuHeap->GetCPUDescriptorHandleForHeapStart();
    m_ResidentGpuHandle0 = m_GpuHeap->GetGPUDescriptorHandleForHeapStart();

    uint32_t dynamicSegmentOffset = RESIDENT_DESCRIPTOR_COUNT * m_DescriptorSize;
    m_DynamicCpuHandle0.ptr = m_ResidentCpuHandle0.ptr + dynamicSegmentOffset;
    m_DynamicGpuHandle0.ptr = m_ResidentGpuHandle0.ptr + dynamicSegmentOffset;
}

uint32_t ResourceDescriptorManager::AllocateResidentDescriptor()
{
    uint32_t index = InterlockedIncrement(&m_NextFreeResidentDescriptorIndex) - 1;

    //TODO: Right now there is no mechanism for freeing reisdent descriptors, they're allocated for the lifetime of the GraphicsCore.
    if (index >= RESIDENT_DESCRIPTOR_COUNT)
    {
        Fail("Reisdent descriptor heap exhausted.");
    }

    return index;
}

DynamicResourceDescriptor ResourceDescriptorManager::AllocateDynamicDescriptor()
{
    return DynamicResourceDescriptor(*this, AllocateResidentDescriptor());
}

ResourceDescriptor ResourceDescriptorManager::CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC& description)
{
    DynamicResourceDescriptor handle = AllocateDynamicDescriptor();
    handle.UpdateConstantBufferView(description);
    return handle.ResourceDescriptor();
}

ResourceDescriptor ResourceDescriptorManager::CreateShaderResourceView(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& description)
{
    DynamicResourceDescriptor handle = AllocateDynamicDescriptor();
    handle.UpdateShaderResourceView(resource, description);
    return handle.ResourceDescriptor();
}

ResourceDescriptor ResourceDescriptorManager::CreateUnorderedAccessView(ID3D12Resource* resource, ID3D12Resource* counterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& description)
{
    DynamicResourceDescriptor handle = AllocateDynamicDescriptor();
    handle.UpdateUnorderedAccessView(resource, counterResource, description);
    return handle.ResourceDescriptor();
}

std::tuple<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> ResourceDescriptorManager::AllocateUninitializedResidentDescriptor()
{
    DynamicResourceDescriptor handle = AllocateDynamicDescriptor();
    return { handle.m_ResidentCpuHandle, handle.m_ResourceDescriptor.ResidentHandle() };
}

DynamicDescriptorTableBuilder ResourceDescriptorManager::AllocateDynamicTable(uint32_t length)
{
    Assert(length > 1); // Dynamic tables must have more than one entry.

TryAgain:
    uint64_t tableEnd = InterlockedAdd64((int64_t*)&m_NextFreeDynamicDescriptorIndex, length);

    // Handle overflow of NextFreeDynamicDescriptorIndex
    if (tableEnd < length)
    {
        goto TryAgain;
    }

    uint64_t tableStart = tableEnd - length;

    //TODO: Ensure that the allocated table does not crash into the back end of descriptors which are still in use
    // Right now we rely on the fact that the dynamic descriptor table is very large, even if it's just for validation it'd be nice to validate.

    // Create the table builder
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_DynamicCpuHandle0;
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_DynamicGpuHandle0;
    uint64_t handleOffset = tableStart * m_DescriptorSize;
    cpuHandle.ptr += (size_t)handleOffset;
    gpuHandle.ptr += handleOffset;

    return {
        m_Graphics.Device(),
        m_DescriptorSize,
        cpuHandle,
        gpuHandle,
        length
    };
}
