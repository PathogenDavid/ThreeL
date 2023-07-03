#include "pch.h"
#include "Texture.h"

#include "GraphicsCore.h"
#include "GraphicsContext.h"
#include "UploadQueue.h"

Texture::Texture(const GraphicsCore& graphics, std::wstring debugName, std::span<const uint8_t> colorData, uint2 size, int texelSize, DXGI_FORMAT format)
{
    Assert(size.x > 0 && size.y > 0);
    Assert(colorData.size_bytes() >= (size.x * size.y * texelSize) && "The specified span is not long enough to contain an RGBA texture of the specified size.");

    // Allocate the texture resource and upload the texture
    D3D12_RESOURCE_DESC textureDescription =
    {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = (UINT64)size.x,
        .Height = (UINT)size.y,
        .DepthOrArraySize = 1,
        .MipLevels = 1, //TODO: Generate mips
        .Format = format,
        .SampleDesc = { .Count = 1 },
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        .Flags = D3D12_RESOURCE_FLAG_NONE,
    };

    PendingUpload pendingUpload = graphics.GetUploadQueue().AllocateResource(textureDescription, debugName);

    Assert(pendingUpload.RowCount() == size.y);
    uint32_t sourceRowPitch = size.x * texelSize;
    for (uint32_t y = 0; y < (uint32_t)size.y; y++)
    {
        std::span<const uint8_t> sourceSpan = colorData.subspan(y * sourceRowPitch, sourceRowPitch);
        std::span<uint8_t> destinationSpan = pendingUpload.GetRow(y);
        Assert(sourceSpan.size() == destinationSpan.size());
        SpanCopy(destinationSpan, sourceSpan);
    }

    // Initiate the upload
    InitiatedUpload upload = pendingUpload.InitiateUpload();

    // Create the SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDescription =
    {
        .Format = textureDescription.Format,
        .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
        .Texture2D =
        {
            .MostDetailedMip = 0,
            .MipLevels = textureDescription.MipLevels,
            .PlaneSlice = 0,
            .ResourceMinLODClamp = 0.f,
        },
    };
    m_SrvHandle = graphics.GetResourceDescriptorManager().CreateShaderResourceView(upload.Resource.Get(), srvDescription);
    m_BindlessIndex = graphics.GetResourceDescriptorManager().GetResidentIndex(m_SrvHandle);

    m_UploadSyncPoint = upload.SyncPoint;
    m_Resource = std::move(upload.Resource);
}
