#include "pch.h"
#include "PbrMaterial.h"

#include "GltfLoadContext.h"
#include "ResourceManager.h"
#include "Texture.h"

#include <tiny_gltf.h>

PbrMaterial::PbrMaterial(GltfLoadContext& context, int materialIndex)
{
    const tinygltf::Material& material = context.Model().materials[materialIndex];
    PbrMaterialParams pbr =
    {
        .AlphaCutoff = -1000.f,
        .BaseColorTexture = TEXTURE_DISABLED,
        .BaseColorTextureSampler = TEXTURE_DISABLED,
        .MealicRoughnessTexture = TEXTURE_DISABLED,
        .MetalicRoughnessTextureSampler = TEXTURE_DISABLED,
        .NormalTexture = TEXTURE_DISABLED,
        .NormalTextureSampler = TEXTURE_DISABLED,
        .NormalTextureScale = 1.f,
        .BaseColorFactor = float4::One,
        .MetallicFactor = 1.f,
        .RoughnessFactor = 1.f,
        .EmissiveTexture = TEXTURE_DISABLED,
        .EmissiveTextureSampler = TEXTURE_DISABLED,
        .EmissiveFactor = float3::Zero,
    };

    // pbrMetallicRoughness
    {
        const std::vector<double>& c = material.pbrMetallicRoughness.baseColorFactor;
        Assert(c.size() == 4);
        pbr.BaseColorFactor = float4((float)c[0], (float)c[1], (float)c[2], (float)c[3]);
    }

    {
        const tinygltf::TextureInfo& colorTexture = material.pbrMetallicRoughness.baseColorTexture;
        if (colorTexture.index >= 0)
        {
            Assert(colorTexture.texCoord == 0 && "Using secondary UV sets is not yet supported.");
            pbr.BaseColorTexture = context.LoadTexture(colorTexture.index, true).BindlessIndex();
            pbr.BaseColorTextureSampler = context.LoadSamplerForTexture(colorTexture.index);
        }
    }

    pbr.MetallicFactor = (float)material.pbrMetallicRoughness.metallicFactor;
    pbr.RoughnessFactor = (float)material.pbrMetallicRoughness.roughnessFactor;

    {
        const tinygltf::TextureInfo& mrTexture = material.pbrMetallicRoughness.metallicRoughnessTexture;
        if (mrTexture.index >= 0)
        {
            Assert(mrTexture.texCoord == 0 && "Using secondary UV sets is not yet supported.");
            pbr.MealicRoughnessTexture = context.LoadTexture(mrTexture.index, false).BindlessIndex();
            pbr.MetalicRoughnessTextureSampler = context.LoadSamplerForTexture(mrTexture.index);
        }
    }

    // normalTexture
    {
        const tinygltf::NormalTextureInfo& normalTexture = material.normalTexture;
        if (normalTexture.index >= 0)
        {
            Assert(normalTexture.texCoord == 0 && "Using secondary UV sets is not yet supported.");
            pbr.NormalTexture = context.LoadTexture(normalTexture.index, false).BindlessIndex();
            pbr.NormalTextureSampler = context.LoadSamplerForTexture(normalTexture.index);
            pbr.NormalTextureScale = (float)normalTexture.scale;
        }
    }

    // occlusionTexture
    {
        Assert(material.occlusionTexture.index == -1 && "Occlusion textures are not supported.");
    }

    // emissiveTexture
    {
        const tinygltf::TextureInfo& emissiveTexture = material.emissiveTexture;
        if (emissiveTexture.index >= 0)
        {
            Assert(emissiveTexture.texCoord == 0 && "Using secondary UV sets is not yet supported.");
            pbr.EmissiveTexture = context.LoadTexture(emissiveTexture.index, true).BindlessIndex();
            pbr.EmissiveTextureSampler = context.LoadSamplerForTexture(emissiveTexture.index);
        }
    }

    // emissiveFactor
    {
        const std::vector<double>& c = material.emissiveFactor;
        Assert(c.size() == 3);
        pbr.EmissiveFactor = float3((float)c[0], (float)c[1], (float)c[2]);
    }

    // alphaCutoff
    if (material.alphaMode == "MASK")
    {
        pbr.AlphaCutoff = (float)material.alphaCutoff;
    }

    // Create the material
    ResourceManager& resources = context.Resources();
    m_MaterialId = context.Resources().PbrMaterials.CreateMaterial(pbr);

    // Select the PSO
    if (material.alphaMode == "BLEND")
    {
        m_IsTransparent = true;
        m_PipelineStateObject = &(material.doubleSided ? resources.PbrBlendOnDoubleSided : resources.PbrBlendOnSingleSided);
    }
    else
    {
        m_IsTransparent = false;
        m_PipelineStateObject = &(material.doubleSided ? resources.PbrBlendOffDoubleSided : resources.PbrBlendOffSingleSided);
    }
}
