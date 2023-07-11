#include "pch.h"
#include "Texture.h"

#include "ComputeContext.h"
#include "GraphicsCore.h"
#include "ResourceManager.h"
#include "ShaderInterop.h"
#include "UploadQueue.h"

#include <pix3.h>

Texture::Texture(const ResourceManager& resources, std::wstring debugName, std::span<const uint8_t> colorData, uint2 size, int texelSize, DXGI_FORMAT format)
{
    Assert(size.x > 0 && size.y > 0);
    Assert(colorData.size_bytes() >= (size.x * size.y * texelSize) && "The specified span is not long enough to contain an RGBA texture of the specified size.");
    GraphicsCore& graphics = resources.Graphics;

    uint16_t mipCount = (uint16_t)log2(std::max(size.x, size.y)) + 1;

    DXGI_FORMAT uavFormat;
    // Ideally we'd have a helper for this so that we can properly support arbitrary formats (these are just the ones currently possible through Texture's public constructors.)
    // However it's more complicated than a 1:1 lookup. Ideally we should have a code path where we allocate a secondary texture for creating the mipmap chain when the format has no UAV equivalent
    // and then copy it over (or we should just always do that to avoid marking all textures as requiring UAV access.)
    // https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/format-support-for-direct3d-11-0-feature-level-hardware
    // https://learn.microsoft.com/en-us/windows/win32/direct3d12/typed-unordered-access-view-loads
    switch (format)
    {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            uavFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            break;
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            uavFormat = format;
            break;
        default:
            mipCount = 1;
            uavFormat = DXGI_FORMAT_UNKNOWN;
            wprintf(L"Texture '%s' (%dx%d) will not have a mipmap chain generated! (Format cannot be mapped as UAV %s)\n", debugName.c_str(), size.x, size.y, DxgiFormat::NameW(format));
            break;
    }

    // Allocate the texture resource and upload the texture
    D3D12_RESOURCE_DESC textureDescription =
    {
        .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
        .Alignment = 0,
        .Width = (UINT64)size.x,
        .Height = (UINT)size.y,
        .DepthOrArraySize = 1,
        .MipLevels = mipCount,
        .Format = format,
        .SampleDesc = { .Count = 1 },
        .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        //TODO: Ideally we should allocate a staging resource in which we generate the mip chain and then copy it to the actual texture
        // Documentation claims some GPUs don't like it when textures have unordered access enabled and that's the only thing we use it for
        .Flags = mipCount > 1 ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE,
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

    // Generate mipmap chain
    if (textureDescription.MipLevels > 1)
    {
        bool isUnorm = DxgiFormat::IsUnorm(textureDescription.Format);
        bool isSrgb = DxgiFormat::IsSrgb(textureDescription.Format);

        ComputeContext context(graphics.GetComputeQueue(), resources.GenerateMipMapsRootSignature, isUnorm ? resources.GenerateMipMapsUnorm : resources.GenerateMipMapsFloat);
        PIXBeginEvent(&context, 0, L"Generate mipmap chain for '%s'", debugName.c_str());

        // Create an SRV and UAV for each mip level
        //TODO: This could actually be a nice use of the vestigual dynamic resource descriptor infrastructure we have floating around,
        // although it was designed with copying from the CPU staging descriptor heap in mind so it's not a perfect fit.
        // For now we just allocate forever descriptors even though they'll only really be used for this one thing
        std::vector<ResourceDescriptor> mipSrvs(textureDescription.MipLevels);
        std::vector<ResourceDescriptor> mipUavs(textureDescription.MipLevels);
        {
            srvDescription.Texture2D.MipLevels = 1;

            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDescription =
            {
                .Format = uavFormat,
                .ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D,
                .Texture2D =
                {
                    .MipSlice = 0,
                    .PlaneSlice = 0,
                },
            };

            for (uint16_t level = 0; level < textureDescription.MipLevels; level++)
            {
                srvDescription.Texture2D.MostDetailedMip = level;
                uavDescription.Texture2D.MipSlice = level;

                mipSrvs[level] = graphics.GetResourceDescriptorManager().CreateShaderResourceView(m_Resource.Get(), srvDescription);

                if (level > 0)
                { mipUavs[level] = graphics.GetResourceDescriptorManager().CreateUnorderedAccessView(m_Resource.Get(), nullptr, uavDescription); }
            }
        }

        // Wait for upload of LOD 0 to finish
        graphics.GetComputeQueue().AwaitSyncPoint(m_UploadSyncPoint);

        uint2 mipSize = size;
        for (uint16_t level = 1; level < textureDescription.MipLevels; level++)
        {
            // Determine mip size
            mipSize = Math::Max(mipSize / 2, uint2(1));

            // Set up shader inputs
            ShaderInterop::GenerateMipmapChainParams params =
            {
                .OutputSize = mipSize,
                .OutputSizeInverse = 1.f / (float2)mipSize,
                .OutputIsSrgb = isSrgb ? 1u : 0u,
            };
            context->SetComputeRoot32BitConstants(ShaderInterop::GenerateMipmapChain::RpParams, sizeof(params) / sizeof(UINT), &params, 0);

            context->SetComputeRootDescriptorTable(ShaderInterop::GenerateMipmapChain::RpInputTexture, mipSrvs[level - 1].GetResidentHandle());
            context->SetComputeRootDescriptorTable(ShaderInterop::GenerateMipmapChain::RpOutputTexture, mipUavs[level].GetResidentHandle());

            // Transition the output texture to allow unordered access
            context.__AllocateResourceBarrier() =
            {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Transition =
                {
                    .pResource = m_Resource.Get(),
                    .Subresource = level,
                    .StateBefore = D3D12_RESOURCE_STATE_COMMON,
                    .StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                },
            };

            // Dispatch compute shader to generate the level N mip from the level N-1 mip
            uint2 dispatchSize = (mipSize + (ShaderInterop::GenerateMipmapChain::ThreadGroupSize - 1)) / ShaderInterop::GenerateMipmapChain::ThreadGroupSize;
            context.Dispatch(uint3(dispatchSize, 1));

            // Flush UAV writes and transition the subresource back to a shader resource
            context.UavBarrier();
            context.__AllocateResourceBarrier() =
            {
                .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
                .Transition =
                {
                    .pResource = m_Resource.Get(),
                    .Subresource = level,
                    .StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                    // Common is required here as the texture will be accessed on the graphics queue later so we need implicit state promotion
                    .StateAfter = D3D12_RESOURCE_STATE_COMMON,
                },
            };
        }

        PIXEndEvent(&context);
        context.Finish();
    }
}
