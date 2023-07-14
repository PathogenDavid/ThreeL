#pragma once
#include <d3d12.h>

/// <summary>Represents a resident descriptor allocated from <see cref="ResourceDescriptorManager"/>.</summary>
struct ResourceDescriptor
{
private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_StagingHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_ResidentHandle;

public:
    ResourceDescriptor()
        : m_StagingHandle(), m_ResidentHandle()
    { }

    ResourceDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE stagingHandle, D3D12_GPU_DESCRIPTOR_HANDLE residentHandle)
        : m_StagingHandle(stagingHandle), m_ResidentHandle(residentHandle)
    { }

    inline D3D12_CPU_DESCRIPTOR_HANDLE StagingHandle() const { return m_StagingHandle; }
    inline D3D12_GPU_DESCRIPTOR_HANDLE ResidentHandle() const { return m_ResidentHandle; }
};
