#pragma once
#include "pch.h"
#include "GpuSyncPoint.h"
#include "Math.h"
#include "ShaderInterop.h"

class GraphicsCore;

using PbrMaterialId = uint32_t;

class PbrMaterialHeap
{
private:
    GraphicsCore& m_Graphics;

    ComPtr<ID3D12Resource> m_MaterialParams;
    std::vector<ShaderInterop::PbrMaterialParams> m_MaterialParamsStaging;

public:
    PbrMaterialHeap(GraphicsCore& graphics);

    PbrMaterialId CreateMaterial(const ShaderInterop::PbrMaterialParams& params);
    GpuSyncPoint UploadMaterials();

    inline D3D12_GPU_VIRTUAL_ADDRESS BufferGpuAddress() const
    {
        Assert(m_MaterialParams != nullptr && "Materials not uploaded to GPU!");
        return m_MaterialParams->GetGPUVirtualAddress();
    }
};
