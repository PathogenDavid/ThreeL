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
        float3 EyePosition;
        uint32_t LightLinkedListBufferWidth;
        uint32_t LightLinkedListBufferShift;
        float DeltaTime;
        uint32_t LightCount;
        uint32_t _Padding;
        float4x4 ViewProjectionTransformInverse;
        float4x4 ViewTransformInverse;
    };
    static_assert(sizeof(PerFrameCb) == 224);
    static_assert(offsetof(PerFrameCb, ViewProjectionTransform) == 0);
    static_assert(offsetof(PerFrameCb, EyePosition) == 64);
    static_assert(offsetof(PerFrameCb, LightLinkedListBufferWidth) == 76);
    static_assert(offsetof(PerFrameCb, LightLinkedListBufferShift) == 80);
    static_assert(offsetof(PerFrameCb, DeltaTime) == 84);
    static_assert(offsetof(PerFrameCb, LightCount) == 88);
    static_assert(offsetof(PerFrameCb, ViewProjectionTransformInverse) == 96);
    static_assert(offsetof(PerFrameCb, ViewTransformInverse) == 160);

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
    static_assert(offsetof(PerNodeCb, ColorsIndex) == 116);
    static_assert(offsetof(PerNodeCb, TangentsIndex) == 120);

    namespace Pbr
    {
        // See PBR_ROOT_SIGNATURE in Common.hlsli
        enum RootParameters
        {
            RpPerNodeCb,
            RpPerFrameCb,
            RpMaterialHeap,
            RpLightHeap,
            RpLightLinksHeap,
            RpFirstLightLinkBuffer,
            RpSamplerHeap,
            RpBindlessHeap,
        };
    }

    const static uint32_t SizeOfParticleState = 32;
    const static uint32_t SizeOfParticleSprite = 48;

    namespace ParticleRender
    {
        // See PBR_ROOT_SIGNATURE in ParticleRender.hlsl
        enum RootParameters
        {
            RpParticleBuffer,
            RpPerFrameCb,
            RpMaterialHeap,
            RpLightHeap,
            RpLightLinksHeap,
            RpFirstLightLinkBuffer,
            RpSamplerHeap,
            RpBindlessHeap,
        };
    }

    struct ParticleSystemParams
    {
        uint32_t ParticleCapacity;
        uint32_t ToSpawnThisFrame;
    };
    static_assert(sizeof(ParticleSystemParams) == 2 * sizeof(uint32_t));
    static_assert(offsetof(ParticleSystemParams, ParticleCapacity) == 0);
    static_assert(offsetof(ParticleSystemParams, ToSpawnThisFrame) == 4);

    namespace ParticleSystem
    {
        // See ROOT_SIGNATURE in ParticleSystem.cs.hlsl
        enum RootParameters
        {
            RpParams,
            RpPerFrameCb,
            RpParticleStatesIn,
            RpLivingParticleCount,
            RpParticleStatesOut,
            RpParticleSpritesOut,
            RpLivingParticleCountOut,
            RpDrawIndirectArguments,
        };

        static const uint32_t SpawnGroupSize = 64;
        static const uint32_t UpdateGroupSize = 64;
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

    namespace DepthDownsample
    {
        // See ROOT_SIGNATURE in DepthDownsample.ps.hlsl
        enum RootParameters
        {
            RpInputDepthBuffer,
        };
    }

    struct LightInfo
    {
        float3 Position;
        float Range;
        float3 Color;
        float Intensity;
    };
    static_assert(sizeof(LightInfo) == 32);
    static_assert(offsetof(LightInfo, Position) == 0);
    static_assert(offsetof(LightInfo, Range) == 12);
    static_assert(offsetof(LightInfo, Color) == 16);
    static_assert(offsetof(LightInfo, Intensity) == 28);

    const static uint32_t SizeOfLightLink = sizeof(uint32_t) * 2;

    struct LightLinkedListFillParams
    {
        uint32_t LightLinksLimit;
        float RangeExtensionRatio;
    };
    static_assert(sizeof(LightLinkedListFillParams) == 2 * sizeof(uint32_t));
    static_assert(offsetof(LightLinkedListFillParams, LightLinksLimit) == 0);
    static_assert(offsetof(LightLinkedListFillParams, RangeExtensionRatio) == 4);

    namespace LightLinkedListFill
    {
        // See LLL_FILL_ROOT_SIGNATURE in LightLinkedListFill.hlsl
        enum RootParameters
        {
            RpFillParams,
            RpPerFrameCb,
            RpLightHeap,
            RpDepthBuffer,
            RpLightLinksHeap,
            RpFirstLightLinkBuffer,
        };
    }

    static const char* LightLinkedListDebugModeNames = "None\0Light count\0Average light color\0Nearest light\0";
    enum class LightLinkedListDebugMode : uint32_t
    {
        None,
        LightCount,
        AverageLightColor,
        NearestLight,
    };

    struct LightLinkedListDebugParams
    {
        LightLinkedListDebugMode Mode;
        uint32_t MaxLightsPerPixel;
        float DebugOverlayAlpha;
    };
    static_assert(sizeof(LightLinkedListDebugParams) == 3 * sizeof(uint32_t));
    static_assert(offsetof(LightLinkedListDebugParams, Mode) == 0);
    static_assert(offsetof(LightLinkedListDebugParams, MaxLightsPerPixel) == 4);
    static_assert(offsetof(LightLinkedListDebugParams, DebugOverlayAlpha) == 8);

    namespace LightLinkedListDebug
    {
        // See LLL_DEBUG_ROOT_SIGNATURE in LightLinkedListDebug.ps.hlsl
        enum RootParameters
        {
            RpDebugParams,
            RpPerFrameCb,
            RpLightHeap,
            RpLightLinksHeap,
            RpFirstLightLinkBuffer,
        };
    }

    namespace LightLinkedListStats
    {
        // See LLL_DEBUG_ROOT_SIGNATURE in LightLinkedListStats.cs.hlsl
        enum RootParameters
        {
            RpParams,
            RpLightLinksHeapCounter,
            RpResults,
            RpLightLinksHeap,
            RpFirstLightLinkBuffer,
        };

        static const uint32_t ThreadGroupSize = 1024;
    }

    namespace LightSprites
    {
        enum RootParameters
        {
            RpPerFrameCb,
            RpLightHeap,
            RpTexture,
        };
    }
}
