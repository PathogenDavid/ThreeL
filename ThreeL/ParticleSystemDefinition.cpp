#include "pch.h"
#include "ParticleSystemDefinition.h"

#include "GraphicsCore.h"
#include "ResourceManager.h"
#include "Texture.h"

#include <stb_image.h>

ParticleSystemDefinition::ParticleSystemDefinition(ResourceManager& resources)
    : m_Resources(resources)
{
    D3D12_SAMPLER_DESC description =
    {
        .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        .AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        .MipLODBias = 0.f,
        .MaxAnisotropy = 0,
        .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
        .BorderColor = { },
        .MinLOD = 0.f,
        .MaxLOD = D3D12_FLOAT32_MAX,
    };
    m_Sampler = resources.Graphics.SamplerHeap().Create(description);
}

void ParticleSystemDefinition::AddParticleMaterial(ShaderInterop::PbrMaterialParams& particlePbr)
{
    PbrMaterialId material = m_Resources.PbrMaterials.CreateMaterial(particlePbr);

    if (m_MinMaterialId == -1)
    {
        m_MinMaterialId = m_MaxMaterialId = material;
        return;
    }

    if (material != m_MaxMaterialId + 1)
    {
        fprintf(stderr, "Failed to add particle material: Particle material IDs must be continuous! (Don't add unrelated materials until finishing particle system.)\n");
        fprintf(stderr, "    New material is #%d, last was #%d\n", material, m_MaxMaterialId);
        Assert(false);
        return;
    }

    m_MaxMaterialId = material;
}

void ParticleSystemDefinition::AddParticleTexture(const std::string& filePath)
{
    int width;
    int height;
    int channels;
    uint8_t* data = stbi_load(filePath.c_str(), &width, &height, &channels, 4);
    Assert(data != nullptr);
    channels = 4;

    // We interpret the particles as being white with the grayscale as alpha
    // Looking at Kenny's Unity sample it seems he intends 
    for (int i = 0; i < width * height * channels; i += channels)
    {
        data[i + 3] = data[i];
        data[i + 0] = 255;
        data[i + 1] = 255;
        data[i + 2] = 255;
    }

    uint32_t textureId = m_ParticleTextures.emplace_back(m_Resources, std::format(L"{}", filePath), std::span(data, width * height * channels), uint2((uint32_t)width, (uint32_t)height), false).BindlessIndex();
    stbi_image_free(data);

    ShaderInterop::PbrMaterialParams particlePbr =
    {
        .AlphaCutoff = -1000.f,
        .BaseColorTexture = textureId,
        .BaseColorTextureSampler = m_Sampler,
        .MealicRoughnessTexture = BUFFER_DISABLED,
        .MetalicRoughnessTextureSampler = BUFFER_DISABLED,
        .NormalTexture = BUFFER_DISABLED,
        .NormalTextureSampler = BUFFER_DISABLED,
        .NormalTextureScale = 1.f,
        .BaseColorFactor = float4::One,
        .MetallicFactor = 0.f,
        .RoughnessFactor = 1.f,
        .EmissiveTexture = BUFFER_DISABLED,
        .EmissiveTextureSampler = BUFFER_DISABLED,
        .EmissiveFactor = float3::Zero,
    };
    AddParticleMaterial(particlePbr);
}

ShaderInterop::ParticleSystemParams ParticleSystemDefinition::CreateShaderParams(uint32_t particleSystemCapacity, uint32_t toSpawnThisFrame, float3 spawnPoint) const
{
    Assert(m_MinMaterialId != -1 && "PBR materials and/or textures must be added to this system before use!");
    return ShaderInterop::ParticleSystemParams
    {
        .ParticleCapacity = particleSystemCapacity,
        .ToSpawnThisFrame = toSpawnThisFrame,
        .MaxSize = MaxSize,
        .FadeOutTime = FadeOutTime,
        .LifeMin = LifeMin,
        .LifeMax = LifeMax,
        .AngularVelocityMin = AngularVelocityMin,
        .AngularVelocityMax = AngularVelocityMax,
        .VelocityDirectionVariance = VelocityDirectionVariance,
        .VelocityMagnitudeMin = VelocityMagnitudeMin,
        .VelocityDirectionBias = VelocityDirectionBias,
        .VelocityMagnitudeMax = VelocityMagnitudeMax,
        .SpawnPoint = spawnPoint,
        .MinMaterialId = m_MinMaterialId,
        .SpawnPointVariance = SpawnPointVariance,
        .MaxMaterialId = m_MaxMaterialId,
        .BaseColor = BaseColor,
        .MinShade = MinShade,
        .MaxShade = MaxShade,
    };
}
