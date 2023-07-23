#include "pch.h"
#include "UavCounter.h"

#include "GraphicsCore.h"

UavCounter::UavCounter(GraphicsCore& graphics, const std::wstring& debugName)
{
    D3D12_HEAP_PROPERTIES heapProperties = { D3D12_HEAP_TYPE_DEFAULT };
    D3D12_RESOURCE_DESC resourceDescription = DescribeBufferResource(sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    AssertSuccess(graphics.Device()->CreateCommittedResource
    (
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDescription,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&m_Resource)
    ));
    m_Resource->SetName(debugName.c_str());

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDescription =
    {
        .Format = DXGI_FORMAT_R32_TYPELESS,
        .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
        .Buffer =
        {
            .FirstElement = 0,
            .NumElements = 1,
            .Flags = D3D12_BUFFER_UAV_FLAG_RAW,
        },
    };
    m_Uav = graphics.ResourceDescriptorManager().CreateUnorderedAccessView(m_Resource.Get(), nullptr, uavDescription);;

    m_GpuAddress = m_Resource->GetGPUVirtualAddress();
}
