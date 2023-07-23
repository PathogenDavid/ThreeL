#define PBR_ROOT_SIGNATURE \
    "SRV(t0, space = 900, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE)," \
    "CBV(b1)," \
    "SRV(t0, flags = DATA_STATIC)," \
    "SRV(t1, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE)," \
    "SRV(t2, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE, visibility = SHADER_VISIBILITY_PIXEL)," \
    "SRV(t3, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE, visibility = SHADER_VISIBILITY_PIXEL)," \
    "DescriptorTable(" \
        "Sampler(s0, space = 1, offset = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE)" \
    ")," \
    "DescriptorTable(" \
        "SRV(t0, space = 2, offset = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE | DATA_VOLATILE)," \
        "SRV(t0, space = 3, offset = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE | DATA_VOLATILE)" \
    ")," \
    ""

#define PBR_IS_PARTICLE

#include "ParticleCommon.hlsli"
#include "Common.hlsli"
#include "Pbr.hlsl"

StructuredBuffer<ParticleSprite> g_Particles : register(t0, space900);

//===================================================================================================================================================
// Vertex shader
//===================================================================================================================================================

[RootSignature(PBR_ROOT_SIGNATURE)]
PsInput VsMainParticle(uint vertexId : SV_VertexID, uint particleIndex : SV_InstanceID)
{
    PsInput result;
    ParticleSprite particle = g_Particles[particleIndex];

    result.Uv0 = float2(uint2(vertexId >> 1, vertexId) & 1.xx);

    float2 corner = mul(lerp(float2(-1.f, 1.f), float2(1.f, -1.f), result.Uv0), particle.Transform);
    float3 cornerOffset = float3(corner, 0.f);
    cornerOffset = mul(cornerOffset, (float3x3)g_PerFrame.ViewTransformInverse);

    result.Position = float4(particle.WorldPosition + cornerOffset, 1.f);
    result.WorldPosition = result.Position.xyz;
    result.Position = mul(result.Position, g_PerFrame.ViewProjectionTransform);

    result.Normal = normalize(mul(float3(0.f, 0.f, -1.f), (float3x3)g_PerFrame.ViewTransformInverse));
    result.Tangent = float4(normalize(mul(float3(1.f, 0.f, 0.f), (float3x3)g_PerFrame.ViewTransformInverse)), 1.f);

    result.Color = particle.Color;

    result.MaterialId = particle.MaterialId;

    return result;
}
