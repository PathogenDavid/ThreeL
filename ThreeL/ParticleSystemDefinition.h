#pragma once
#include "PbrMaterial.h"
#include "SamplerHeap.h"
#include "Vector3.h"

#include <vector>

struct ResourceManager;
class Texture;

struct ParticleSystemDefinition
{
public:
    float SpawnRate = 1.f; // # of particles spawned per second

    float MaxSize = 1.f;

    float FadeOutTime = 1.f;
    float LifeMin = 1.f;
    float LifeMax = 1.f;

    float AngularVelocityMin = 0.f;
    float AngularVelocityMax = 0.f;

    float3 VelocityDirectionVariance = float3::Zero;
    float3 VelocityDirectionBias = float3::Zero;
    float VelocityMagnitudeMin = 0.f;
    float VelocityMagnitudeMax = 0.f;

    float3 SpawnPointVariance = float3::Zero;

    float3 BaseColor = float3::One;
    float MinShade = 1.f;
    float MaxShade = 1.f;

private:
    ResourceManager& m_Resources;
    std::vector<Texture> m_ParticleTextures;
    SamplerId m_Sampler;

    PbrMaterialId m_MinMaterialId = -1;
    PbrMaterialId m_MaxMaterialId = -1;

public:
    ParticleSystemDefinition(ResourceManager& resources);
    void AddParticleMaterial(ShaderInterop::PbrMaterialParams& particlePbr);
    void AddParticleTexture(const std::string& filePath);

    ShaderInterop::ParticleSystemParams CreateShaderParams(uint32_t particleSystemCapacity, uint32_t toSpawnThisFrame, float3 spawnPoint) const;
};
