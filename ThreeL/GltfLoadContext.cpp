#include "pch.h"
#include "GltfLoadContext.h"

#include "GltfAccessorView.h"
#include "GraphicsCore.h"
#include "Scene.h"
#include "Texture.h"

static D3D12_TEXTURE_ADDRESS_MODE TranslateGltfAddressMode(int mode)
{
    switch (mode)
    {
        case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case TINYGLTF_TEXTURE_WRAP_REPEAT: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        default:
            Assert(false && "Invalid glTF texture address mode!");
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    }
}

// Note: This function doesn't cache since SamplerHeap already has its own caching
SamplerId GltfLoadContext::LoadSampler(int samplerIndex)
{
    // The spec only defines the default values for the address mode as wrap and leaves the filtering modes up to the implementation
    // Linear is sensible so we go with that
    // https://github.com/KhronosGroup/glTF/blob/5de957b8b0a13c147c90d4ff569250440931872f/specification/2.0/README.md#sampler
    // https://github.com/KhronosGroup/glTF/blob/5de957b8b0a13c147c90d4ff569250440931872f/specification/2.0/README.md#texturesampler
    D3D12_SAMPLER_DESC description =
    {
        .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        .AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        .MipLODBias = 0.f,
        .MaxAnisotropy = 0,
        .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
        .BorderColor = { },
        .MinLOD = 0.f,
        .MaxLOD = D3D12_FLOAT32_MAX,
    };

    if (samplerIndex >= 0)
    {
        const tinygltf::Sampler& sampler = m_Model.samplers[samplerIndex];

        D3D12_FILTER_TYPE min = D3D12_DECODE_MIN_FILTER(description.Filter);
        D3D12_FILTER_TYPE mag = D3D12_DECODE_MAG_FILTER(description.Filter);
        D3D12_FILTER_TYPE mip = D3D12_DECODE_MIP_FILTER(description.Filter);
        D3D12_FILTER_REDUCTION_TYPE reduction = D3D12_DECODE_FILTER_REDUCTION(description.Filter);

        switch (sampler.minFilter)
        {
            case TINYGLTF_TEXTURE_FILTER_NEAREST:
                min = D3D12_FILTER_TYPE_POINT;
                mip = D3D12_FILTER_TYPE_POINT;
                description.MaxLOD = 0.f;
                break;
            case TINYGLTF_TEXTURE_FILTER_LINEAR:
                min = D3D12_FILTER_TYPE_LINEAR;
                mip = D3D12_FILTER_TYPE_POINT;
                description.MaxLOD = 0.f;
                break;
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
                min = D3D12_FILTER_TYPE_POINT;
                mip = D3D12_FILTER_TYPE_POINT;
                break;
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
                min = D3D12_FILTER_TYPE_LINEAR;
                mip = D3D12_FILTER_TYPE_POINT;
                break;
            case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
                min = D3D12_FILTER_TYPE_POINT;
                mip = D3D12_FILTER_TYPE_LINEAR;
                break;
            case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
                min = D3D12_FILTER_TYPE_LINEAR;
                mip = D3D12_FILTER_TYPE_LINEAR;
                break;
            default:
                Assert(sampler.minFilter == -1 && "glTF sampler's minification filter is invalid!");
        }

        switch (sampler.magFilter)
        {
            case TINYGLTF_TEXTURE_FILTER_NEAREST:
                mag = D3D12_FILTER_TYPE_POINT;
                break;
            case TINYGLTF_TEXTURE_FILTER_LINEAR:
                mag = D3D12_FILTER_TYPE_LINEAR;
                break;
            default:
                Assert(sampler.magFilter == -1 && "glTF sampler's magnification filter is invalid!");
        }

        description.Filter = D3D12_ENCODE_BASIC_FILTER(min, mag, mip, reduction);
        description.AddressU = TranslateGltfAddressMode(sampler.wrapS);
        description.AddressV = TranslateGltfAddressMode(sampler.wrapT);
    }

    return m_Graphics.GetSamplerHeap().Create(description);
}

const Texture& GltfLoadContext::LoadTexture(int textureIndex, bool isSrgb)
{
    const tinygltf::Texture& gltfTexture = m_Model.textures[textureIndex];

    // Spec allows this for textures backed by extensions, but we don't support it.
    Assert(gltfTexture.source >= 0 && "glTF texture is not backed by an image!");

    // Return an existing texture if we've already uploaded this one
    // Even though it's unlikely a texture will be shared between sRGB textures (IE: baseColorTexture/emissiveTexture)
    // and linear textures but we make it part of the cache key just inc ase
    const uint64_t kIsSrgbBit = 1ull << 32;
    uint64_t cacheKey = gltfTexture.source | (isSrgb ? kIsSrgbBit : 0);
    auto cached = m_Scene.m_TextureCache.find(cacheKey);
    if (cached != m_Scene.m_TextureCache.end())
    {
        return *(cached->second);
    }

    // Validate image attributes
    const tinygltf::Image& image = m_Model.images[gltfTexture.source];
    Assert(image.width > 0);
    Assert(image.height > 0);
    Assert(image.component == 4 && "Loaded image is not 32bpp, TinyGLTF should not be configured to preserve source channels.");
    Assert(image.bits == 8 || image.bits == 16);
    Assert(image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE || image.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT);

    // Make a debug name for the texture
    std::wstring name = std::format(L"{}", gltfTexture.name);

    if (image.name.length() > 0)
    { name = name.length() > 0 ? std::format(L"{}/{}", name, image.name) : std::format(L"{}", image.name); }

    // Fall back to the URI (if it's short -- IE: not a data URI) or just 'unnamed'
    if (name.length() == 0)
    { name = image.uri.length() > 0 && image.uri.length() < 32 ? std::format(L"{}", image.uri) : L"Unnamed"; }

    name = std::format(L"{}#{}/{}", name, textureIndex, gltfTexture.source);

    if (isSrgb)
    { name = std::format(L"{} (sRGB)", name); }

    // Create, cache, and return the texture
    return *(m_Scene.m_TextureCache[cacheKey] = std::make_unique<Texture>(m_Resources, name, std::span(image.image), uint2((uint32_t)image.width, (uint32_t)image.height), isSrgb));
}

GltfAccessorViewBase* GltfLoadContext::GetAttributeRaw(const tinygltf::Primitive& primitive, const std::string& attributeName)
{
    auto accessorIndex = primitive.attributes.find(attributeName);
    if (accessorIndex == primitive.attributes.end())
    {
        return nullptr;
    }

    return GltfAccessorViewBase::Create(m_Model, m_Model.accessors[accessorIndex->second]);
}
