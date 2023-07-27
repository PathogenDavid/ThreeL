#pragma once
#include "GpuSyncPoint.h"
#include "RawGpuResource.h"
#include "ResourceDescriptor.h"
#include "UavCounter.h"
#include "Vector3.h"

struct ComputeContext;
struct GraphicsContext;
class GraphicsCore;
class LightHeap;
class LightLinkedList;
struct ParticleSystemDefinition;
struct ResourceManager;

class ParticleSystem
{
private:
    ResourceManager& m_Resources;
    GraphicsCore& m_Graphics;
    std::wstring m_DebugName;
    const ParticleSystemDefinition& m_Definition;
    float3 m_SpawnPoint;
    uint32_t m_Capacity;

    float m_SpawnLeftover = 0.f;

    GpuSyncPoint m_UpdateSyncPoint;
    GpuSyncPoint m_RenderSyncPoint;

    struct ParticleStateBuffer
    {
        RawGpuResource Buffer;
        ResourceDescriptor Uav;
        UavCounter Counter;
    };
    ParticleStateBuffer m_ParticleStateBuffers[2];
    uint32_t m_CurrentParticleStateBuffer = 0;

    RawGpuResource m_ParticleSpriteBuffer;

    RawGpuResource m_ParticleSpriteSortBuffer;
    ResourceDescriptor m_ParticleSpriteSortBufferUav;

    RawGpuResource m_DrawIndirectArguments;

public:
    ParticleSystem(ResourceManager& resources, const std::wstring& debugName, const ParticleSystemDefinition& definition, float3 spawnPoint, uint32_t capacity);

private:
    void Update(ComputeContext& context, float deltaTime, D3D12_GPU_VIRTUAL_ADDRESS perFrameCb, bool skipPrepareRender);
public:
    inline void Update(ComputeContext& context, float deltaTime, D3D12_GPU_VIRTUAL_ADDRESS perFrameCb)
    { Update(context, deltaTime, perFrameCb, false); }

    void Render(GraphicsContext& context, D3D12_GPU_VIRTUAL_ADDRESS perFrameCb, LightHeap& lightHeap, LightLinkedList& lightLinkedList, bool showLightBoundaries = false);

    //! Seeds the state of the particle system by simulating it for the specified number of (simulated) seconds
    void SeedState(float numSeconds);

    void Reset(ComputeContext& context);

    inline float3 SpawnPoint() const { return m_SpawnPoint; }
    inline void SpawnPoint(float3 spawnPoint) { m_SpawnPoint = spawnPoint; }
};
