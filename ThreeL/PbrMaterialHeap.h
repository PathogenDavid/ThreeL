#pragma once
#include "pch.h"
#include "GpuSyncPoint.h"
#include "Math.h"

class GraphicsCore;

#define TEXTURE_DISABLED 0xFFFFFFFF

using PbrMaterialId = uint32_t;

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

class PbrMaterialHeap
{
private:
    GraphicsCore& m_Graphics;

    ComPtr<ID3D12Resource> m_MaterialParams;
    std::vector<PbrMaterialParams> m_MaterialParamsStaging;

public:
    PbrMaterialHeap(GraphicsCore& graphics);

    PbrMaterialId CreateMaterial(const PbrMaterialParams& params);
    GpuSyncPoint UploadMaterials();

    inline D3D12_GPU_VIRTUAL_ADDRESS BufferGpuAddress() const
    {
        Assert(m_MaterialParams != nullptr && "Materials not uploaded to GPU!");
        return m_MaterialParams->GetGPUVirtualAddress();
    }
};
