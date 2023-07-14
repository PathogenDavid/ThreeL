#include "pch.h"
#include "DynamicResourceDescriptor.h"

#include "GraphicsCore.h"
#include "ResourceDescriptorManager.h"

DynamicResourceDescriptor::DynamicResourceDescriptor(const ResourceDescriptorManager& manager, uint32_t index)
{
    m_Device = manager.m_Graphics.Device();
    D3D12_CPU_DESCRIPTOR_HANDLE stagingHandle = manager.m_StagingHeapHandle0;
    m_ResidentCpuHandle = manager.m_ResidentCpuHandle0;
    D3D12_GPU_DESCRIPTOR_HANDLE residentGpuHandle = manager.m_ResidentGpuHandle0;

    uint32_t offset = index * manager.m_DescriptorSize;
    stagingHandle.ptr += offset;
    m_ResidentCpuHandle.ptr += offset;
    residentGpuHandle.ptr += offset;

    m_ResourceDescriptor = { stagingHandle, residentGpuHandle };
}
