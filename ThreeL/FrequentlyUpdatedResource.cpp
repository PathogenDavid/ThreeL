#include "pch.h"
#include "FrequentlyUpdatedResource.h"

FrequentlyUpdatedResource::FrequentlyUpdatedResource(GraphicsCore& graphics, const D3D12_RESOURCE_DESC& resourceDescription, const std::wstring& debugName)
    : m_UploadQueue(graphics.UploadQueue())
{
    Assert(resourceDescription.Dimension > D3D12_RESOURCE_DIMENSION_UNKNOWN && resourceDescription.Dimension <= D3D12_RESOURCE_DIMENSION_TEXTURE3D);
    Assert(debugName.length() > 0);

    m_IsTextureUpload = resourceDescription.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER;

    // Describe the resident resource
    D3D12_HEAP_PROPERTIES resourceHeapProperties = { D3D12_HEAP_TYPE_DEFAULT };

    // Determine the layout of the upload resource
    UINT rowCount;
    UINT64 rowSizeBytes;
    graphics.Device()->GetCopyableFootprints(&resourceDescription, 0, 1, 0, &m_UploadPlacedFootprint, &rowCount, &rowSizeBytes, &m_UploadBufferSize);

    // Describe the upload resource
    D3D12_HEAP_PROPERTIES uploadHeapProperties = { D3D12_HEAP_TYPE_UPLOAD };
    D3D12_RESOURCE_DESC uploadResourceDescription =
    {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = m_UploadBufferSize,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = { .Count = 1 },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE,
    };

    // Allocate the resources
    for (uint32_t i = 0; i < RESOURCE_COUNT; i++)
    {
        ComPtr<ID3D12Resource> resource;
        AssertSuccess(graphics.Device()->CreateCommittedResource
        (
            &resourceHeapProperties,
            D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
            &resourceDescription,
            // Since we're going to first access this resource from a copy queue, it needs to start in the common state rather than copy destintation
            // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states#constants
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&resource)
        ));
        resource->SetName(std::format(L"{} #{}", debugName, i).c_str());
        m_Resources[i] = std::move(resource);

        // Create the upload buffer
        //TODO: If UploadQueue is ever modified to recycle buffers, it'd probably make more sense to borrow its buffers instead of juggling our own.
        ComPtr<ID3D12Resource> uploadResource;
        AssertSuccess(graphics.Device()->CreateCommittedResource(
            &uploadHeapProperties,
            D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
            &uploadResourceDescription,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&uploadResource)
        ));
        uploadResource->SetName(std::format(L"{} #{} (Upload)", debugName, i).c_str());
        m_UploadResources[i] = std::move(uploadResource);
    }
}

GpuSyncPoint FrequentlyUpdatedResource::Update(const std::span<const uint8_t> data)
{
    Assert(data.size_bytes() <= m_UploadBufferSize && "The specified data span exceeds the size of this resource.");
    Assert(!m_IsTextureUpload || data.size_bytes() == m_UploadBufferSize && "Textures must be updated in their entirity");

    m_CurrentResource++;
    if (m_CurrentResource >= RESOURCE_COUNT)
    { m_CurrentResource = 0; }

    // Assert this resource has no outstanding uploads
#ifdef DEBUG
    m_LastUploadSyncPoints[m_CurrentResource].AssertReached();
#endif

    ID3D12Resource* uploadResource = m_UploadResources[m_CurrentResource].Get();
    ID3D12Resource* resource = m_Resources[m_CurrentResource].Get();

    // Update the upload resource
    {
        uint8_t* mappedPtr;
        D3D12_RANGE emptyRange = { }; // We aren't going to read anything
        AssertSuccess(uploadResource->Map(0, &emptyRange, (void**)&mappedPtr));

        size_t mappedSpanSize = m_UploadBufferSize - m_UploadPlacedFootprint.Offset;
        mappedPtr += m_UploadPlacedFootprint.Offset;

        std::span<uint8_t> mappedSpan(mappedPtr, mappedSpanSize);
        SpanCopy(mappedSpan, data);

        D3D12_RANGE writtenRange = { 0, data.size_bytes() };
        uploadResource->Unmap(0, &writtenRange);
    }

    // Perform the upload
    //TODO: Textures need to be explicitly transitioned back to COPY_DEST before upload. Who's responsibility should this be?
    GpuSyncPoint syncPoint;
    if (m_IsTextureUpload)
    { syncPoint = m_UploadQueue.PerformTextureUpload(resource, uploadResource, m_UploadPlacedFootprint); }
    else
    { syncPoint = m_UploadQueue.PerformBufferUpload(resource, uploadResource, data.size_bytes()); }

#ifdef DEBUG
    m_LastUploadSyncPoints[m_CurrentResource] = syncPoint;
#endif
    return syncPoint;
}
