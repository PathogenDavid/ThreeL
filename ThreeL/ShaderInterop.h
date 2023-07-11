#pragma once
#include "Math.h"

#define BUFFER_DISABLED 0xFFFFFFFF

namespace ShaderInterop
{
    struct PbrMaterialParams
    {
        float AlphaCutoff;
        uint32_t BaseColorTexture;
        uint32_t BaseColorTextureSampler;
        uint32_t MealicRoughnessTexture;
        uint32_t MetalicRoughnessTextureSampler;
        uint32_t NormalTexture;
        uint32_t NormalTextureSampler;
        float NormalTextureScale;

        float4 BaseColorFactor;

        float MetallicFactor;
        float RoughnessFactor;

        uint32_t EmissiveTexture;
        uint32_t EmissiveTextureSampler;
        float3 EmissiveFactor;
    };
    static_assert(sizeof(PbrMaterialParams) == 76);
    static_assert(offsetof(PbrMaterialParams, AlphaCutoff) == 0);
    static_assert(offsetof(PbrMaterialParams, BaseColorTexture) == 4);
    static_assert(offsetof(PbrMaterialParams, BaseColorTextureSampler) == 8);
    static_assert(offsetof(PbrMaterialParams, MealicRoughnessTexture) == 12);
    static_assert(offsetof(PbrMaterialParams, MetalicRoughnessTextureSampler) == 16);
    static_assert(offsetof(PbrMaterialParams, NormalTexture) == 20);
    static_assert(offsetof(PbrMaterialParams, NormalTextureSampler) == 24);
    static_assert(offsetof(PbrMaterialParams, NormalTextureScale) == 28);
    static_assert(offsetof(PbrMaterialParams, BaseColorFactor) == 32);
    static_assert(offsetof(PbrMaterialParams, MetallicFactor) == 48);
    static_assert(offsetof(PbrMaterialParams, RoughnessFactor) == 52);
    static_assert(offsetof(PbrMaterialParams, EmissiveTexture) == 56);
    static_assert(offsetof(PbrMaterialParams, EmissiveTextureSampler) == 60);
    static_assert(offsetof(PbrMaterialParams, EmissiveFactor) == 64);

    struct PerFrameCb
    {
        float4x4 ViewProjectionTransform;
    };
    static_assert(sizeof(PerFrameCb) == 16 * sizeof(uint32_t));

    struct PerNodeCb
    {
        float4x4 Transform;
        float3x3 NormalTransform;
        uint32_t MaterialId;
        uint32_t ColorsIndex;
        uint32_t TangentsIndex;
    };
    static_assert(sizeof(PerNodeCb) == 31 * sizeof(uint32_t));
    static_assert(offsetof(PerNodeCb, Transform) == 0);
    static_assert(offsetof(PerNodeCb, NormalTransform) == 64);
    static_assert(offsetof(PerNodeCb, MaterialId) == 112);

    namespace Pbr
    {
        // See ROOT_SIGNATURE in Pbr.hlsl
        enum RootParameters
        {
            RpPerNodeCb,
            RpPerFrameCb,
            RpMaterialHeap,
            RpSamplerHeap,
            RpBindlessHeap,
        };
    }

    struct GenerateMipmapChainParams
    {
        uint2 OutputSize;
        float2 OutputSizeInverse;
        uint32_t OutputIsSrgb;
    };
    static_assert(sizeof(GenerateMipmapChainParams) == 5 * sizeof(uint32_t));
    static_assert(offsetof(GenerateMipmapChainParams, OutputSize) == 0);
    static_assert(offsetof(GenerateMipmapChainParams, OutputSizeInverse) == 8);
    static_assert(offsetof(GenerateMipmapChainParams, OutputIsSrgb) == 16);

    namespace GenerateMipmapChain
    {
        // See ROOT_SIGNATURE in GenerateMipmapChain.cs.hlsl
        enum RootParameters
        {
            RpParams,
            RpInputTexture,
            RpOutputTexture
        };

        static const uint32_t ThreadGroupSize = 8;
    }
}
