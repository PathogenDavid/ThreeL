#pragma once

#define DISABLED_BUFFER 0xFFFFFFFF

namespace Math
{
    static const float Pi = 3.141592653589793f;
}

struct MaterialParams
{
    float AlphaCutoff;
    uint BaseColorTexture;
    uint BaseColorTextureSampler;
    uint MealicRoughnessTexture;
    uint MetalicRoughnessTextureSampler;
    uint NormalTexture;
    uint NormalTextureSampler;
    float NormalTextureScale;

    float4 BaseColorFactor;

    float MetallicFactor;
    float RoughnessFactor;

    uint EmissiveTexture;
    uint EmissiveTextureSampler;
    float3 EmissiveFactor;
};

struct PerNode
{
    float4x4 Transform;
    float3x3 NormalTransform;
    float _padding; // Ensure MatrialId is in its own row, makes CPU-side struct simpler
    uint MaterialId;
    uint ColorsIndex;
    uint TangentsIndex;
};

struct PerFrame
{
    float4x4 ViewProjectionTransform;
    float3 EyePosition;
};

ConstantBuffer<PerNode> g_PerNode : register(b0);
ConstantBuffer<PerFrame> g_PerFrame : register(b1);
StructuredBuffer<MaterialParams> g_Materials : register(t0);

SamplerState g_Samplers[] : register(space1);
Texture2D g_Textures[] : register(space2);
ByteAddressBuffer g_Buffers[] : register(space3);

#define ROOT_SIGNATURE \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
    "RootConstants(num32BitConstants = 31, b0)," \
    "RootConstants(num32BitConstants = 19, b1)," \
    "SRV(t0, flags = DATA_STATIC)," \
    "DescriptorTable(" \
        "Sampler(s0, space = 1, offset = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE)" \
    ")," \
    "DescriptorTable(" \
        "SRV(t0, space = 2, offset = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE | DATA_VOLATILE)," \
        "SRV(t0, space = 3, offset = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE | DATA_VOLATILE)" \
    ")," \
    ""
