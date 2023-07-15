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

struct LightInfo
{
    float3 Position;
    float Range;
    float3 Color;
    float Intensity;
};

struct LightLink
{
    uint DepthInfo; // minDepth/maxDepth of light encoded as two float16s
    uint LightId_NextLight; // upper 8 bits are light ID, lower 24 are next link index

    float MinDepth() { return f16tof32(DepthInfo); }
    float MaxDepth() { return f16tof32(DepthInfo >> 16); }
    uint LightId() { return LightId_NextLight >> 24; }
    uint NextLightIndex() { return LightId_NextLight & 0xFFFFFF; }
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
    uint LightCount;
    uint LightLinkedListBufferWidth;
    uint LightLinkedListBufferShift;
};

ConstantBuffer<PerNode> g_PerNode : register(b0);
ConstantBuffer<PerFrame> g_PerFrame : register(b1);
StructuredBuffer<MaterialParams> g_Materials : register(t0);
StructuredBuffer<LightInfo> g_Lights : register(t1); // (Do not access in stages before the upload fence is awaited.)

SamplerState g_Samplers[] : register(space1);
Texture2D g_Textures[] : register(space2);
ByteAddressBuffer g_Buffers[] : register(space3);

#define ROOT_SIGNATURE \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
    "RootConstants(num32BitConstants = 31, b0)," \
    "RootConstants(num32BitConstants = 22, b1)," \
    "SRV(t0, flags = DATA_STATIC)," \
    "SRV(t1, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE)," \
    "DescriptorTable(" \
        "Sampler(s0, space = 1, offset = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE)" \
    ")," \
    "DescriptorTable(" \
        "SRV(t0, space = 2, offset = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE | DATA_VOLATILE)," \
        "SRV(t0, space = 3, offset = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE | DATA_VOLATILE)" \
    ")," \
    ""

Texture2D<float> g_DepthBuffer : register(t0, space900);
RWStructuredBuffer<LightLink> g_LightLinksHeapRW : register(u0, space900);
RWByteAddressBuffer g_FirstLightLinkRW : register(u1, space900);

uint2 ScreenSpaceToLightLinkedListSpace(uint2 position)
{
    // Equivalent to `position / pow(2, g_PerFrame.LightLinkedListBufferShift)`
    return position >> g_PerFrame.LightLinkedListBufferShift;
}

// Gets the the address withing_FirstLightLinkRW for the given pixel
// The specified position should be in LLL buffer space.
// IE: If caller is operating in screen space, it must convert the coordinate using ScreenSpaceToLightLinkedListSpace
uint GetFirstLightLinkAddress(uint2 position)
{
    // It might be worth testing to see if storing these links along a Z-order curve would improve performance
    // In theory it'd be more cache-friendly. We could also explore letting the GPU do its thing and go through
    // the extra effort of having g_FirstLightLinkRW be a texture either by copying the buffer to a Texture2D
    // after filling it or by using a standard swizzle on GPUs which support it.
    return (position.x + position.y * g_PerFrame.LightLinkedListBufferWidth) * 4;
}

#define LLL_FILL_ROOT_SIGNATURE \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
    "RootConstants(num32BitConstants = 22, b1)," \
    "SRV(t1, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE)," \
    "DescriptorTable(" \
        "SRV(t0, space = 900, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE)," \
        "visibility = SHADER_VISIBILITY_PIXEL" \
    ")," \
    "DescriptorTable(" \
        "UAV(u0, space = 900, flags = DATA_VOLATILE)," \
        "visibility = SHADER_VISIBILITY_PIXEL" \
    ")," \
    "UAV(u1, space = 900, flags = DATA_VOLATILE)," \
    ""
