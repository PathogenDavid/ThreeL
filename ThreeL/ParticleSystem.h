#pragma once
#include "GpuSyncPoint.h"
#include "RawGpuResource.h"
#include "ResourceDescriptor.h"
#include "UavCounter.h"
#include "Vector3.h"

struct GraphicsContext;
class GraphicsCore;
class LightHeap;
class LightLinkedList;
struct ResourceManager;

class ParticleSystem
{
private:
    ResourceManager& m_Resources;
    GraphicsCore& m_Graphics;
    std::wstring m_DebugName;
    uint32_t m_Capacity;

    float m_SpawnRate;
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

    RawGpuResource m_DrawIndirectArguments;

public:
    ParticleSystem(ResourceManager& resources, const std::wstring& debugName, uint32_t capacity, float spawnRatePerSecond);

    void Update(GraphicsContext& context, float deltaTime, float3 eyePosition, D3D12_GPU_VIRTUAL_ADDRESS perFrameCb);
    void Render(GraphicsContext& context, D3D12_GPU_VIRTUAL_ADDRESS perFrameCb, LightHeap& lightHeap, LightLinkedList& lightLinkedList);
};
