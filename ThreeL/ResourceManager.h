#pragma once
#include "GraphicsCore.h"
#include "MeshHeap.h"
#include "PbrMaterialHeap.h"
#include "PipelineStateObject.h"
#include "RootSignature.h"

namespace MeshInputSlot
{
    enum MeshInputSlot : UINT
    {
        Position = 0,
        Normal = 1,
        Uv0 = 2,
    };
}

struct ResourceManager
{
    GraphicsCore& Graphics;
    PbrMaterialHeap PbrMaterials;
    MeshHeap MeshHeap;

    // No complicated PSO management here, we don't need very many so we just make them all by hand
    RootSignature PbrRootSignature;
    PipelineStateObject PbrBlendOffSingleSided;
    PipelineStateObject PbrBlendOffDoubleSided;
    PipelineStateObject PbrBlendOnSingleSided;
    PipelineStateObject PbrBlendOnDoubleSided;

    RootSignature GenerateMipMapsRootSignature;
    PipelineStateObject GenerateMipMapsUnorm;
    PipelineStateObject GenerateMipMapsFloat;

    explicit ResourceManager(GraphicsCore& graphics);
    ResourceManager(const ResourceManager&) = delete;

    //! Marks this resource manager as finished, flushing its managed resources to the GPU
    //! Does not wait for GPU work to complete, caller is expected to flush the upload queue themselves
    inline void Finish()
    {
        // We don't bother on waiting for any sync points here, caller is expect to flush the upload queue
        PbrMaterials.UploadMaterials();
        MeshHeap.Flush();
    }
};
