#include "ResourceDescriptorManager.h"

#include <new>
#include <Windows.h>

ResourceDescriptorManager::ResourceDescriptorManager(const ComPtr<ID3D12Device>& device)
{
    m_Device = device;
    m_DescriptorSize = m_Device->GetDescriptorHandleIncrementSize(HEAP_TYPE);

    D3D12_DESCRIPTOR_HEAP_DESC heapDescription =
    {
        .Type = HEAP_TYPE,
        .NumDescriptors = RESIDENT_DESCRIPTOR_COUNT,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    };

    AssertSuccess(m_Device->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&m_StagingHeap)));
    m_StagingHeap->SetName(L"ARES CBV/SRV/UAV Descriptor Staging Heap");
    m_StagingHeapHandle0 = m_StagingHeap->GetCPUDescriptorHandleForHeapStart();

    heapDescription.NumDescriptors = TOTAL_DESCRIPTOR_COUNT;
    heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    AssertSuccess(m_Device->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&m_GpuHeap)));
    m_GpuHeap->SetName(L"ARES CBV/SRV/UAV Descriptor GPU Heap");
    m_ResidentCpuHandle0 = m_GpuHeap->GetCPUDescriptorHandleForHeapStart();
    m_ResidentGpuHandle0 = m_GpuHeap->GetGPUDescriptorHandleForHeapStart();

    uint32_t dynamicSegmentOffset = RESIDENT_DESCRIPTOR_COUNT * m_DescriptorSize;
    m_DynamicCpuHandle0.ptr = m_ResidentCpuHandle0.ptr + dynamicSegmentOffset;
    m_DynamicGpuHandle0.ptr = m_ResidentGpuHandle0.ptr + dynamicSegmentOffset;
}

struct UninitializedResidentDescriptorHandle
{
    ID3D12Device* m_Device;
    D3D12_CPU_DESCRIPTOR_HANDLE m_StagingHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE m_ResidentCpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_ResidentGpuHandle;

    UninitializedResidentDescriptorHandle(const ResourceDescriptorManager& manager, uint32_t index)
    {
        m_Device = manager.m_Device.Get();
        m_StagingHandle = manager.m_StagingHeapHandle0;
        m_ResidentCpuHandle = manager.m_ResidentCpuHandle0;
        m_ResidentGpuHandle = manager.m_ResidentGpuHandle0;

        uint32_t offset = index * manager.m_DescriptorSize;
        m_StagingHandle.ptr += offset;
        m_ResidentCpuHandle.ptr += offset;
        m_ResidentGpuHandle.ptr += offset;
    }

    ResourceDescriptor Initialize(D3D12_CONSTANT_BUFFER_VIEW_DESC* description)
    {
        m_Device->CreateConstantBufferView(description, m_StagingHandle);
        return Complete();
    }

    ResourceDescriptor Initialize(ID3D12Resource* resource, D3D12_SHADER_RESOURCE_VIEW_DESC* description)
    {
        m_Device->CreateShaderResourceView(resource, description, m_StagingHandle);
        return Complete();
    }

    ResourceDescriptor Initialize(ID3D12Resource* resource, ID3D12Resource* counterResource, D3D12_UNORDERED_ACCESS_VIEW_DESC* description)
    {
        m_Device->CreateUnorderedAccessView(resource, counterResource, description, m_StagingHandle);
        return Complete();
    }

    ResourceDescriptor Complete()
    {
        m_Device->CopyDescriptorsSimple(1, m_ResidentCpuHandle, m_StagingHandle, ResourceDescriptorManager::HEAP_TYPE);
        return { m_StagingHandle, m_ResidentGpuHandle };
    }
};

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

ResourceDescriptor ResourceDescriptorManager::CreateConstantBufferView(D3D12_CONSTANT_BUFFER_VIEW_DESC* description)
{
    UninitializedResidentDescriptorHandle handle(*this, AllocateResidentDescriptor());
    return handle.Initialize(description);
}

ResourceDescriptor ResourceDescriptorManager::CreateShaderResourceView(ID3D12Resource* resource, D3D12_SHADER_RESOURCE_VIEW_DESC* description)
{
    UninitializedResidentDescriptorHandle handle(*this, AllocateResidentDescriptor());
    return handle.Initialize(resource, description);
}

ResourceDescriptor ResourceDescriptorManager::CreateUnorderedAccessView(ID3D12Resource* resource, ID3D12Resource* counterResource, D3D12_UNORDERED_ACCESS_VIEW_DESC* description)
{
    UninitializedResidentDescriptorHandle handle(*this, AllocateResidentDescriptor());
    return handle.Initialize(resource, counterResource, description);
}

std::tuple<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> ResourceDescriptorManager::AllocateUninitializedResidentDescriptor()
{
    UninitializedResidentDescriptorHandle handle(*this, AllocateResidentDescriptor());
    return { handle.m_ResidentCpuHandle, handle.m_ResidentGpuHandle };
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
        m_Device.Get(),
        m_DescriptorSize,
        cpuHandle,
        gpuHandle,
        length
    };
}

void ResourceDescriptorManager::BindHeap(ID3D12GraphicsCommandList* commandList)
{
    ID3D12DescriptorHeap* heaps[] = { m_GpuHeap.Get() };
    commandList->SetDescriptorHeaps(1, heaps);
}
