#pragma once
#include "DynamicDescriptorTable.h"
#include "ResourceDescriptor.h"
#include "Util.h"

#include <d3d12.h>

struct DynamicDescriptorTableBuilder
{
    friend class ResourceDescriptorManager;

private:
    // Not a ComPtr since this type is ephemeral
    ID3D12Device* m_Device;
    uint32_t m_DescriptorSize;
    D3D12_CPU_DESCRIPTOR_HANDLE m_NextCpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_FirstGpuHandle;
    uint32_t m_Capacity;
    uint32_t m_Count;

    DynamicDescriptorTableBuilder(ID3D12Device* device, uint32_t descriptorSize, D3D12_CPU_DESCRIPTOR_HANDLE firstCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE firstGpuHandle, uint32_t capacity)
    {
        m_Device = device;
        m_DescriptorSize = descriptorSize;
        m_NextCpuHandle = firstCpuHandle;
        m_FirstGpuHandle = firstGpuHandle;
        m_Capacity = capacity;
        m_Count = 0;
    }

public:
    void Add(ResourceDescriptor descriptor);
    DynamicDescriptorTable Finish();
};
