#include "pch.h"
#include "DynamicDescriptorTableBuilder.h"
#include "ResourceDescriptorManager.h"

void DynamicDescriptorTableBuilder::Add(ResourceDescriptor descriptor)
{
    if (m_Count >= m_Capacity)
    {
        Fail("Cannot add descriptors after the table is full!");
    }

    m_Device->CopyDescriptorsSimple(1, m_NextCpuHandle, descriptor.GetStagingHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_NextCpuHandle.ptr += m_DescriptorSize;
    m_Count++;
}

DynamicDescriptorTable DynamicDescriptorTableBuilder::Finish()
{
    if (m_Count < m_Capacity)
    {
        Fail("Cannot mark an incomplete descriptor table as finished.");
    }

    return { m_FirstGpuHandle };
}
