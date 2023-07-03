#pragma once
#include "Math.h"

namespace ShaderInterop
{
    struct PerFrameCb
    {
        float4x4 ViewProjectionTransform;
    };
    static_assert(sizeof(PerFrameCb) == 16 * sizeof(uint32_t));

    struct PerNodeCb
    {
        float4x4 Transform;
        float4x4 NormalTransform;
        uint32_t MaterialId;
    };
    static_assert(sizeof(PerNodeCb) == 33 * sizeof(uint32_t));
    static_assert(offsetof(PerNodeCb, Transform) == 0);
    static_assert(offsetof(PerNodeCb, NormalTransform) == 64);
    static_assert(offsetof(PerNodeCb, MaterialId) == 128);

    namespace Pbr
    {
        enum RootParameters
        {
            RpPerNodeCb,
            RpPerFrameCb,
            RpMaterialHeap,
            RpSamplerHeap,
            RpBindlessHeap,
        };
    }
}
