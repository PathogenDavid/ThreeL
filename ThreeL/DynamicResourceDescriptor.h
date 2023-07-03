#pragma once
#include "ResourceDescriptor.h"

#include <d3d12.h>
#include <stdint.h>

class ResourceDescriptorManager;

struct DynamicResourceDescriptor
{
    friend class ResourceDescriptorManager;

private:
    ID3D12Device* m_Device;
    ResourceDescriptor m_ResourceDescriptor;
    D3D12_CPU_DESCRIPTOR_HANDLE m_ResidentCpuHandle;
    DynamicResourceDescriptor(const ResourceDescriptorManager& manager, uint32_t index);

    inline void CopyToGpu()
    {
        m_Device->CopyDescriptorsSimple(1, m_ResidentCpuHandle, m_ResourceDescriptor.GetStagingHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

public:
    DynamicResourceDescriptor() = default;

    //! Updates this resource descriptor to be the specified constant buffer view.
    //! The current resource descriptor must not be in use by the GPU or undefined behavior occurs.
    inline void UpdateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC& description)
    {
        m_Device->CreateConstantBufferView(&description, m_ResourceDescriptor.GetStagingHandle());
        CopyToGpu();
    }

    //! Updates this resource descriptor to be the specified shader resource view.
    //! The current resource descriptor must not be in use by the GPU or undefined behavior occurs.
    inline void UpdateShaderResourceView(ID3D12Resource* resource, const D3D12_SHADER_RESOURCE_VIEW_DESC& description)
    {
        m_Device->CreateShaderResourceView(resource, &description, m_ResourceDescriptor.GetStagingHandle());
        CopyToGpu();
    }

    //! Updates this resource descriptor to be the specified unordered access view.
    //! The current resource descriptor must not be in use by the GPU or undefined
    inline void UpdateUnorderedAccessView(ID3D12Resource* resource, ID3D12Resource* counterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& description)
    {
        m_Device->CreateUnorderedAccessView(resource, counterResource, &description, m_ResourceDescriptor.GetStagingHandle());
        CopyToGpu();
    }

    //! The resource descriptor targeted by this dynamic descriptor.
    //! Descriptor handle is valid before initialization, but its contents are not.
    inline const ResourceDescriptor& ResourceDescriptor() const { return m_ResourceDescriptor; }
};
