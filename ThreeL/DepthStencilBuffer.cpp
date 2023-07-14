#include "pch.h"
#include "DepthStencilBuffer.h"

#include "GraphicsCore.h"

DepthStencilBuffer::DepthStencilBuffer(GraphicsCore& graphics, const std::wstring& name, uint2 size, DXGI_FORMAT format, float depthClearValue, uint8_t stencilClearValue)
    : m_Graphics(graphics), m_Name(name), m_Size(), m_Format(format), m_DepthClearValue(depthClearValue), m_StencilClearValue(stencilClearValue)
{
    Assert(size.x > 0 && size.y > 0);

    // Determine SRV formats
    switch (format)
    {
        case DXGI_FORMAT_D16_UNORM:
            m_DepthSrvFormat = DXGI_FORMAT_R16_UNORM;
            m_StencilSrvFormat = DXGI_FORMAT_UNKNOWN;
            break;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            m_DepthSrvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            m_StencilSrvFormat = DXGI_FORMAT_X24_TYPELESS_G8_UINT;
            break;
        case DXGI_FORMAT_D32_FLOAT:
            m_DepthSrvFormat = DXGI_FORMAT_R32_FLOAT;
            m_StencilSrvFormat = DXGI_FORMAT_UNKNOWN;
            break;
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            m_DepthSrvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
            m_StencilSrvFormat = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
            break;
        default:
            Assert(false && "The specified format is not valid for depth buffers.");
            m_DepthSrvFormat = DXGI_FORMAT_UNKNOWN;
            m_StencilSrvFormat = DXGI_FORMAT_UNKNOWN;
    }

    // Create the descriptor heap for the view
    //TODO: Ideally we should just have a central heap for this purpose
    D3D12_DESCRIPTOR_HEAP_DESC heapDescription =
    {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
        .NumDescriptors = m_StencilSrvFormat == DXGI_FORMAT_UNKNOWN ? 2u : 4u,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
    };
    AssertSuccess(graphics.GetDevice()->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&m_DescriptorHeap)));
    m_DescriptorHeap->SetName(std::format(L"{} (Descriptor heap)", name).c_str());

    // Allocate descriptors for the SRVs
    m_DepthShaderResourceView = graphics.GetResourceDescriptorManager().AllocateDynamicDescriptor();
    m_StencilShaderResourceView = m_StencilSrvFormat == DXGI_FORMAT_UNKNOWN ? DynamicResourceDescriptor() : graphics.GetResourceDescriptorManager().AllocateDynamicDescriptor();

    // Create the buffer and resource views
    Resize(size);
}

void DepthStencilBuffer::Resize(uint2 newSize)
{
    if ((newSize == m_Size).All())
    {
        Assert(m_Resource != nullptr);
        return;
    }

    ID3D12Device* device = m_Graphics.GetDevice().Get();

    // Release the old buffer if we have one
    m_Resource = nullptr;

    // Allocate the buffer on the GPU
    D3D12_HEAP_PROPERTIES heapProperties = { D3D12_HEAP_TYPE_DEFAULT };
    D3D12_RESOURCE_DESC resourceDescription =
    {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = newSize.x,
        .Height = newSize.y,
        .DepthOrArraySize = 1,
        .MipLevels = 0,
        .Format = m_Format,
        .SampleDesc = { .Count = 1, .Quality = 0 },
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
    };

    D3D12_CLEAR_VALUE optimizedClearValue =
    {
        .Format = m_Format,
        .DepthStencil =
        {
            .Depth = m_DepthClearValue,
            .Stencil = m_StencilClearValue,
        },
    };

    AssertSuccess(device->CreateCommittedResource
    (
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDescription,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &optimizedClearValue,
        IID_PPV_ARGS(&m_Resource)
    ));
    m_Resource->SetName(m_Name.c_str());
    m_CurrentState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    m_Size = newSize;

    // Create the depth-stencil views
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    UINT dsvHandleSize = m_Graphics.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m_View = DepthStencilView(dsvHandle);
    dsvHandle.ptr += dsvHandleSize;
    m_DepthReadOnlyView = DepthStencilView(dsvHandle);

    D3D12_DEPTH_STENCIL_VIEW_DESC viewDescription =
    {
        .Format = m_Format,
        .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
        .Flags = D3D12_DSV_FLAG_NONE,
        .Texture2D = { .MipSlice = 0 },
    };

    device->CreateDepthStencilView(m_Resource.Get(), &viewDescription, m_View);

    viewDescription.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH;
    device->CreateDepthStencilView(m_Resource.Get(), &viewDescription, m_DepthReadOnlyView);

    if (m_StencilSrvFormat == DXGI_FORMAT_UNKNOWN)
    {
        // No stencil, just reuse depth views
        m_StencilReadOnlyView = m_View;
        m_ReadOnlyView = m_DepthReadOnlyView;
    }
    else
    {
        dsvHandle.ptr += dsvHandleSize;
        m_StencilReadOnlyView = DepthStencilView(dsvHandle);
        dsvHandle.ptr += dsvHandleSize;
        m_ReadOnlyView = DepthStencilView(dsvHandle);

        viewDescription.Flags = D3D12_DSV_FLAG_READ_ONLY_STENCIL;
        device->CreateDepthStencilView(m_Resource.Get(), &viewDescription, m_StencilReadOnlyView);

        viewDescription.Flags = D3D12_DSV_FLAG_READ_ONLY_DEPTH | D3D12_DSV_FLAG_READ_ONLY_STENCIL;
        device->CreateDepthStencilView(m_Resource.Get(), &viewDescription, m_ReadOnlyView);
    }

    // Create the shader resource views
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDescription =
    {
        .Format = m_DepthSrvFormat,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D =
        {
            .MostDetailedMip = 0,
            .MipLevels = 1,
            .PlaneSlice = 0,
            .ResourceMinLODClamp = 0,
        },
    };

    m_DepthShaderResourceView.UpdateShaderResourceView(m_Resource.Get(), srvDescription);

    if (m_StencilSrvFormat == DXGI_FORMAT_UNKNOWN)
    { m_StencilShaderResourceView = { }; }
    else
    {
        srvDescription.Format = m_StencilSrvFormat;
        m_StencilShaderResourceView.UpdateShaderResourceView(m_Resource.Get(), srvDescription);
    }
}
