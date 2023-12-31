#pragma once
#include "BitonicSort.h"
#include "MeshHeap.h"
#include "PbrMaterialHeap.h"
#include "PipelineStateObject.h"
#include "RootSignature.h"

class GraphicsCore;

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
    BitonicSort BitonicSort;

    // No complicated PSO management here, we don't need very many so we just make them all by hand
    RootSignature PbrRootSignature;
    PipelineStateObject PbrBlendOffSingleSided;
    PipelineStateObject PbrBlendOffDoubleSided;
    PipelineStateObject PbrBlendOnSingleSided;
    PipelineStateObject PbrBlendOnDoubleSided;

    PipelineStateObject PbrLightDebugSingleSided;
    PipelineStateObject PbrLightDebugDoubleSided;

    RootSignature DepthOnlyRootSignature;
    PipelineStateObject DepthOnlySingleSided;
    PipelineStateObject DepthOnlyDoubleSided;

    RootSignature DepthDownsampleRootSignature;
    PipelineStateObject DepthDownsample;

    RootSignature GenerateMipMapsRootSignature;
    PipelineStateObject GenerateMipMapsUnorm;
    PipelineStateObject GenerateMipMapsFloat;

    RootSignature LightLinkedListFillRootSignature;
    PipelineStateObject LightLinkedListFill;

    RootSignature LightLinkedListDebugRootSignature;
    PipelineStateObject LightLinkedListDebug;

    RootSignature LightLinkedListStatsRootSignature;
    PipelineStateObject LightLinkedListStats;

    RootSignature LightSpritesRootSignature;
    PipelineStateObject LightSprites;

    RootSignature ParticleSystemRootSignature;
    PipelineStateObject ParticleSystemSpawn;
    PipelineStateObject ParticleSystemUpdate;
    PipelineStateObject ParticleSystemPrepareDrawIndirect;

    RootSignature ParticleRenderRootSignature;
    PipelineStateObject ParticleRender;
    PipelineStateObject ParticleRenderLightDebug;

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
