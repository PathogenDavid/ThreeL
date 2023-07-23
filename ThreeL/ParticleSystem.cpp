#include "pch.h"
#include "ParticleSystem.h"

#include "ComputeContext.h"
#include "GraphicsContext.h"
#include "GraphicsCore.h"
#include "LightHeap.h"
#include "LightLinkedList.h"
#include "ResourceManager.h"
#include "ShaderInterop.h"

#include <pix3.h>

// This works but isn't really necessary
// At one point I was getting sporradic inexplicable stuttering from the m_RenderSyncPoint await in Update.
// Those issues mysteriously vanished, but out of an abundance of caution I'm leaving this disabled for the sake of ThreeL working well for demonstration purposes.
// (In practice updating is so fast it's already done before any drawing happens anyway.)
//#define USE_ASYNC_COMPUTE

ParticleSystem::ParticleSystem(ResourceManager& resources, const std::wstring& debugName, uint32_t capacity, float spawnRatePerSecond)
    : m_Resources(resources)
    , m_Graphics(resources.Graphics)
    , m_DebugName(debugName)
    , m_Capacity(capacity)
    , m_SpawnRate(spawnRatePerSecond)
{
    D3D12_HEAP_PROPERTIES heapProperties = { D3D12_HEAP_TYPE_DEFAULT };

    // Allocate state buffers
    D3D12_RESOURCE_DESC stateBufferDescription = DescribeBufferResource(ShaderInterop::SizeOfParticleState * capacity, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

    for (size_t i = 0; i < 2; i++)
    {
        ComPtr<ID3D12Resource> buffer;
        AssertSuccess(m_Graphics.Device()->CreateCommittedResource
        (
            &heapProperties,
            D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
            &stateBufferDescription,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&buffer)
        ));
        buffer->SetName(std::format(L"'{}' Particle States {}", debugName, i).c_str());

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDescription =
        {
            .Format = DXGI_FORMAT_UNKNOWN,
            .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
            .Buffer =
            {
                .FirstElement = 0,
                .NumElements = capacity,
                .StructureByteStride = ShaderInterop::SizeOfParticleState,
                .CounterOffsetInBytes = 0,
                .Flags = D3D12_BUFFER_UAV_FLAG_NONE,
            },
        };

        m_ParticleStateBuffers[i].Counter = UavCounter(m_Graphics, std::format(L"'{}' Particle Counter {}", debugName, i));
        m_ParticleStateBuffers[i].Uav = m_Graphics.ResourceDescriptorManager().CreateUnorderedAccessView(buffer.Get(), m_ParticleStateBuffers[i].Counter, uavDescription);
        m_ParticleStateBuffers[i].Buffer = RawGpuResource(std::move(buffer));
    }

    // Allocate sprite buffer
    D3D12_RESOURCE_DESC spriteBufferDescription = DescribeBufferResource(ShaderInterop::SizeOfParticleSprite * capacity, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    ComPtr<ID3D12Resource> spriteBuffer;
    AssertSuccess(m_Graphics.Device()->CreateCommittedResource
    (
        &heapProperties,
        D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
        &spriteBufferDescription,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&spriteBuffer)
    ));
    spriteBuffer->SetName(std::format(L"'{}' Particle Sprites", debugName).c_str());
    m_ParticleSpriteBuffer = RawGpuResource(std::move(spriteBuffer));

    // Allocate DrawIndirect arguments buffer
    D3D12_RESOURCE_DESC drawIndirectArgumentsDescription = DescribeBufferResource(sizeof(D3D12_DRAW_ARGUMENTS), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    ComPtr<ID3D12Resource> drawIndirectArguments;
    AssertSuccess(m_Graphics.Device()->CreateCommittedResource
    (
        &heapProperties,
        D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
        &drawIndirectArgumentsDescription,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&drawIndirectArguments)
    ));
    drawIndirectArguments->SetName(std::format(L"'{}' Particle Render Arguments", debugName).c_str());
    m_DrawIndirectArguments = RawGpuResource(std::move(drawIndirectArguments));
}

void ParticleSystem::Update(GraphicsContext& graphicsContext, float deltaTime, float3 eyePosition, D3D12_GPU_VIRTUAL_ADDRESS perFrameCb)
{
#ifdef USE_ASYNC_COMPUTE
    // Make sure we don't update buffers on the async compute queue while they're still being used to render the previous frame
    // Could also keep track of additional buffers instead, which would allow us to prepare particles for frame N while frame N - 1 is still rendering
    // (For the sake of ThreeL though this really doesn't even need to use async compute in the first place so I didn't bother.)
    m_Graphics.ComputeQueue().AwaitSyncPoint(m_RenderSyncPoint);

    ComputeContext context(m_Graphics.ComputeQueue(), m_Resources.ParticleSystemRootSignature, m_Resources.ParticleSystemUpdate);
#else
    GraphicsContext& context = graphicsContext;
    context.Flush();
    context->SetComputeRootSignature(m_Resources.ParticleSystemRootSignature);
    context->SetPipelineState(m_Resources.ParticleSystemUpdate);
#endif
    PIXBeginEvent(&context, 42, L"Update '%s' particle system", m_DebugName.c_str());

    // Determine number of particles to spawn this frame
    float toSpawnF = m_SpawnLeftover + m_SpawnRate * deltaTime;
    uint32_t toSpawn = (uint32_t)toSpawnF;
    m_SpawnLeftover = Math::Frac(toSpawnF); // Accumulate partial unspawned particles for future frames

    // Choose state buffers
    ParticleStateBuffer& inputStateBuffer = m_ParticleStateBuffers[m_CurrentParticleStateBuffer];
    m_CurrentParticleStateBuffer = m_CurrentParticleStateBuffer == 0 ? 1 : 0;
    ParticleStateBuffer& outputStateBuffer = m_ParticleStateBuffers[m_CurrentParticleStateBuffer];

    // Transition resources and reset the particle counter
    context.TransitionResource(inputStateBuffer.Buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    context.TransitionResource(inputStateBuffer.Counter, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    context.TransitionResource(outputStateBuffer.Buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    context.TransitionResource(outputStateBuffer.Counter, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    context.TransitionResource(m_ParticleSpriteBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    context.TransitionResource(m_DrawIndirectArguments, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    context.ClearUav(outputStateBuffer.Counter);

    // Bind root signature
    ShaderInterop::ParticleSystemParams params =
    {
        .ParticleCapacity = m_Capacity,
        .ToSpawnThisFrame = toSpawn,
    };
    context->SetComputeRoot32BitConstants(ShaderInterop::ParticleSystem::RpParams, sizeof(params) / sizeof(uint32_t), &params, 0);
    context->SetComputeRootConstantBufferView(ShaderInterop::ParticleSystem::RpPerFrameCb, perFrameCb);
    context->SetComputeRootShaderResourceView(ShaderInterop::ParticleSystem::RpParticleStatesIn, inputStateBuffer.Buffer.GpuAddress());
    context->SetComputeRootShaderResourceView(ShaderInterop::ParticleSystem::RpLivingParticleCount, inputStateBuffer.Counter.GpuAddress());
    context->SetComputeRootDescriptorTable(ShaderInterop::ParticleSystem::RpParticleStatesOut, outputStateBuffer.Uav.ResidentHandle());
    context->SetComputeRootUnorderedAccessView(ShaderInterop::ParticleSystem::RpParticleSpritesOut, m_ParticleSpriteBuffer.GpuAddress());
    context->SetComputeRootUnorderedAccessView(ShaderInterop::ParticleSystem::RpLivingParticleCountOut, outputStateBuffer.Counter.GpuAddress());
    context->SetComputeRootUnorderedAccessView(ShaderInterop::ParticleSystem::RpDrawIndirectArguments, m_DrawIndirectArguments.GpuAddress());

    // Update existing particles
    //PERF: For systems with very large capacities that are not near capacity, this ends up spawning a bunch of useless threads
    // Could use indirect dispatch in order to only spawn the number needed to update the living particle count (which we don't know down on the CPU.)
    context.Dispatch(Math::DivRoundUp(m_Capacity, ShaderInterop::ParticleSystem::UpdateGroupSize));

    // Spawn new particles
    // Spawning happens after updating so that we only spawn new particles when there's free particle slots
    if (toSpawn > 0)
    {
        context.UavBarrier(outputStateBuffer.Counter); // Update must finish allocating output particles before spawning can happen
        context->SetPipelineState(m_Resources.ParticleSystemSpawn);
        context.Dispatch(Math::DivRoundUp(toSpawn, ShaderInterop::ParticleSystem::SpawnGroupSize));
    }

    // Prepare parameters for the indirect draw
    context.UavBarrier(outputStateBuffer.Counter); // We need to know the final particle count by this point
    context->SetPipelineState(m_Resources.ParticleSystemPrepareDrawIndirect);
    context.Dispatch(1);

    // Sort particle sprites
    //TODO

    // Transition all resources for their use in render
    context.UavBarrier();
    context.TransitionResource(m_ParticleSpriteBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    context.TransitionResource(m_DrawIndirectArguments, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

    // Update complete, save a sync point for render
    PIXEndEvent(&context);
#ifdef USE_ASYNC_COMPUTE
    m_UpdateSyncPoint = context.Finish();
#endif
}

void ParticleSystem::Render(GraphicsContext& context, D3D12_GPU_VIRTUAL_ADDRESS perFrameCb, LightHeap& lightHeap, LightLinkedList& lightLinkedList)
{
    PIXBeginEvent(&context, 42, L"Render '%s' particle system", m_DebugName.c_str());

    // Ensure particle update has completed on the async compute queue
#ifdef USE_ASYNC_COMPUTE
    m_Graphics.GraphicsQueue().AwaitSyncPoint(m_UpdateSyncPoint);
#endif

    // Draw the particles
    context->SetGraphicsRootSignature(m_Resources.ParticleRenderRootSignature);

    context->SetGraphicsRootShaderResourceView(ShaderInterop::ParticleRender::RpParticleBuffer, m_ParticleSpriteBuffer.GpuAddress());
    context->SetGraphicsRootConstantBufferView(ShaderInterop::ParticleRender::RpPerFrameCb, perFrameCb);
    context->SetGraphicsRootShaderResourceView(ShaderInterop::ParticleRender::RpMaterialHeap, m_Resources.PbrMaterials.BufferGpuAddress());

    context->SetGraphicsRootShaderResourceView(ShaderInterop::ParticleRender::RpLightHeap, lightHeap.BufferGpuAddress());
    context->SetGraphicsRootShaderResourceView(ShaderInterop::ParticleRender::RpLightLinksHeap, lightLinkedList.LightLinksHeapGpuAddress());
    context->SetGraphicsRootShaderResourceView(ShaderInterop::ParticleRender::RpFirstLightLinkBuffer, lightLinkedList.FirstLightLinkBufferGpuAddress());

    context->SetGraphicsRootDescriptorTable(ShaderInterop::ParticleRender::RpSamplerHeap, m_Graphics.SamplerHeap().GpuHeap()->GetGPUDescriptorHandleForHeapStart());
    context->SetGraphicsRootDescriptorTable(ShaderInterop::ParticleRender::RpBindlessHeap, m_Graphics.ResourceDescriptorManager().GpuHeap()->GetGPUDescriptorHandleForHeapStart());

    context->SetPipelineState(m_Resources.ParticleRender);
    context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context.DrawIndirect(m_DrawIndirectArguments);

    PIXEndEvent(&context);
#ifdef USE_ASYNC_COMPUTE
    m_RenderSyncPoint = context.Flush();
#endif
}
