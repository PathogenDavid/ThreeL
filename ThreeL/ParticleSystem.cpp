#include "pch.h"
#include "ParticleSystem.h"

#include "ComputeContext.h"
#include "DebugLayer.h"
#include "GraphicsContext.h"
#include "GraphicsCore.h"
#include "LightHeap.h"
#include "LightLinkedList.h"
#include "ParticleSystemDefinition.h"
#include "ResourceManager.h"
#include "ShaderInterop.h"

#include <pix3.h>

ParticleSystem::ParticleSystem(ResourceManager& resources, const std::wstring& debugName, const ParticleSystemDefinition& definition, float3 spawnPoint, uint32_t capacity)
    : m_Resources(resources)
    , m_Graphics(resources.Graphics)
    , m_DebugName(debugName)
    , m_Definition(definition)
    , m_SpawnPoint(spawnPoint)
    , m_Capacity(capacity)
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

    // Allocate sort buffer
    uint32_t sortBufferSizeBytes = sizeof(uint2) * m_Capacity;
    D3D12_RESOURCE_DESC sortBufferDescription = DescribeBufferResource(sortBufferSizeBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    ComPtr<ID3D12Resource> sortBuffer;
    AssertSuccess(m_Graphics.Device()->CreateCommittedResource
    (
        &heapProperties,
        D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
        &sortBufferDescription,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&sortBuffer)
    ));
    sortBuffer->SetName(std::format(L"'{}' Particle Sort Buffer", debugName).c_str());

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDescription =
    {
        .Format = DXGI_FORMAT_R32_TYPELESS,
        .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
        .Buffer =
        {
            .FirstElement = 0,
            .NumElements = sortBufferSizeBytes / sizeof(uint32_t),
            .Flags = D3D12_BUFFER_UAV_FLAG_RAW,
        },
    };

    m_ParticleSpriteSortBufferUav = m_Graphics.ResourceDescriptorManager().CreateUnorderedAccessView(sortBuffer.Get(), nullptr, uavDescription);
    m_ParticleSpriteSortBuffer = RawGpuResource(std::move(sortBuffer));

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

void ParticleSystem::Update(ComputeContext& context, float deltaTime, D3D12_GPU_VIRTUAL_ADDRESS perFrameCb, bool skipPrepareRender)
{
    bool isAsyncCompute = context.QueueType() == D3D12_COMMAND_LIST_TYPE_COMPUTE;
    if (isAsyncCompute)
    {
        // Make sure we don't update buffers on the async compute queue while they're still being used to render the previous frame
        // Could also keep track of additional buffers instead, which would allow us to prepare particles for frame N while frame N - 1 is still rendering
        // (For the sake of ThreeL though this really doesn't even need to use async compute in the first place so I didn't bother.)
        m_Graphics.ComputeQueue().AwaitSyncPoint(m_RenderSyncPoint);
    }

    context->SetComputeRootSignature(m_Resources.ParticleSystemRootSignature);
    context->SetPipelineState(m_Resources.ParticleSystemUpdate);

    PIXBeginEvent(&context, 42, L"Update '%s' particle system", m_DebugName.c_str());

    // Determine number of particles to spawn this frame
    float toSpawnF = m_SpawnLeftover + m_Definition.SpawnRate * deltaTime;
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
    ShaderInterop::ParticleSystemParams params = m_Definition.CreateShaderParams(m_Capacity, toSpawn, m_SpawnPoint);
    context->SetComputeRoot32BitConstants(ShaderInterop::ParticleSystem::RpParams, sizeof(params) / sizeof(uint32_t), &params, 0);
    context->SetComputeRootConstantBufferView(ShaderInterop::ParticleSystem::RpPerFrameCb, perFrameCb);
    context->SetComputeRootShaderResourceView(ShaderInterop::ParticleSystem::RpParticleStatesIn, inputStateBuffer.Buffer.GpuAddress());
    context->SetComputeRootShaderResourceView(ShaderInterop::ParticleSystem::RpLivingParticleCount, inputStateBuffer.Counter.GpuAddress());
    context->SetComputeRootDescriptorTable(ShaderInterop::ParticleSystem::RpParticleStatesOut, outputStateBuffer.Uav.ResidentHandle());
    context->SetComputeRootUnorderedAccessView(ShaderInterop::ParticleSystem::RpParticleSpritesOut, m_ParticleSpriteBuffer.GpuAddress());
    context->SetComputeRootUnorderedAccessView(ShaderInterop::ParticleSystem::RpLivingParticleCountOut, outputStateBuffer.Counter.GpuAddress());
    context->SetComputeRootUnorderedAccessView(ShaderInterop::ParticleSystem::RpDrawIndirectArguments, m_DrawIndirectArguments.GpuAddress());
    context->SetComputeRootUnorderedAccessView(ShaderInterop::ParticleSystem::RpParticleSpriteSortBuffer, m_ParticleSpriteSortBuffer.GpuAddress());

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

    // Don't bother preparing to render if we aren't going to do it
    if (skipPrepareRender)
    {
        context.UavBarrier();
        PIXEndEvent(&context);

        if (isAsyncCompute)
        { m_UpdateSyncPoint = context.Flush(); }
        return;
    }

    // Prepare parameters for the indirect draw
    context.UavBarrier(outputStateBuffer.Counter); // We need to know the final particle count by this point
    context->SetPipelineState(m_Resources.ParticleSystemPrepareDrawIndirect);
    context.Dispatch(1);

    // Sort particle sprites
    BitonicSortParams sortParams =
    {
        .SortList = m_ParticleSpriteSortBuffer,
        .SortListUav = m_ParticleSpriteSortBufferUav,
        .Capacity = m_Capacity,
        .ItemKind = BitonicSortParams::SeparateKeyIndex,
        .ItemCountBuffer = outputStateBuffer.Counter,
        .SkipPreSort = false,
        .SortAscending = false,
    };
    m_Resources.BitonicSort.Sort(context, sortParams);

    // Transition all resources for their use in render
    context.UavBarrier();
    context.TransitionResource(m_ParticleSpriteBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    context.TransitionResource(m_ParticleSpriteSortBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    context.TransitionResource(m_DrawIndirectArguments, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);

    // Update complete, save a sync point for render
    PIXEndEvent(&context);
    if (isAsyncCompute)
    { m_UpdateSyncPoint = context.Flush(); }
}

void ParticleSystem::Render(GraphicsContext& context, D3D12_GPU_VIRTUAL_ADDRESS perFrameCb, LightHeap& lightHeap, LightLinkedList& lightLinkedList)
{
    PIXBeginEvent(&context, 42, L"Render '%s' particle system", m_DebugName.c_str());

    // Ensure particle update has completed on the async compute queue
    // (If work wasn't done on the async compute queue this is a no-op)
    if (!m_UpdateSyncPoint.WasReached())
    { m_Graphics.GraphicsQueue().AwaitSyncPoint(m_UpdateSyncPoint); }

    // Draw the particles
    context->SetGraphicsRootSignature(m_Resources.ParticleRenderRootSignature);

    context->SetGraphicsRootShaderResourceView(ShaderInterop::ParticleRender::RpParticleBuffer, m_ParticleSpriteBuffer.GpuAddress());
    context->SetGraphicsRootShaderResourceView(ShaderInterop::ParticleRender::RpSortedParticleLookupBuffer, m_ParticleSpriteSortBuffer.GpuAddress());
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
    m_RenderSyncPoint = context.Flush();
}

void ParticleSystem::SeedState(float numSeconds)
{
    // Run the forced simulation just often enough to ensure particles spawn when they "should"
    // (But cap it at 30 updates per simulated second in case spawn rate is very high)
    float simulatedRate = std::max(1.f / m_Definition.SpawnRate, 1.f / 30.f);
    uint32_t framesToSimulate = (uint32_t)(std::ceil(numSeconds / simulatedRate) + 0.5f);

    struct AlignedPerFrameCb
    {
        ShaderInterop::PerFrameCb PerFrameCb;
        uint8_t Padding[D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - (sizeof(ShaderInterop::PerFrameCb) % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)];
    };

    // Create fake per-frame constant buffers
    D3D12_RESOURCE_DESC perFrameCbBufferDescription = DescribeBufferResource(sizeof(AlignedPerFrameCb) * framesToSimulate);
    PendingUpload pendingUpload = m_Graphics.UploadQueue().AllocateResource(perFrameCbBufferDescription, std::format(L"'{}' ParticleSystem::SeedState PerFrameCb buffer", m_DebugName));

    // Start at a distance frame so that the random number seeds don't overlap with the start
    uint32_t fakeStartFrame = std::numeric_limits<uint32_t>::max() - framesToSimulate;
    std::span<AlignedPerFrameCb> perFrames = SpanCast<uint8_t, AlignedPerFrameCb>(pendingUpload.StagingBuffer());
    float numSecondsSim = numSeconds;
    for (uint32_t i = 0; i < framesToSimulate; i++)
    {
        perFrames[i].PerFrameCb = ShaderInterop::PerFrameCb
        {
            // A lot of these are provided with dummy values as they're unused by the basic stages of updating
            .ViewProjectionTransform = float4x4::Identity,
            .EyePosition = float3::Zero,
            .LightLinkedListBufferWidth = 100,
            .LightLinkedListBufferShift = 0,
            .DeltaTime = std::min(simulatedRate, numSecondsSim),
            .FrameNumber = fakeStartFrame + i,
            .LightCount = 0,
            .ViewProjectionTransformInverse = float4x4::Identity,
            .ViewTransformInverse = float4x4::Identity,
        };
        numSecondsSim -= simulatedRate;
    }

    InitiatedUpload upload = pendingUpload.InitiateUpload();
    ComPtr<ID3D12Resource> perFrameCbs = std::move(upload.Resource);

    // Simulate the particle system
    ComputeContext context(m_Graphics.GraphicsQueue());
    m_Graphics.GraphicsQueue().AwaitSyncPoint(upload.SyncPoint);
    PIXBeginEvent(&context, 0, L"ParticleSystem::SeedState for '%s'", m_DebugName.c_str());

    numSecondsSim = numSeconds;
    uint32_t lastFrame = framesToSimulate - 1;
    D3D12_GPU_VIRTUAL_ADDRESS perFrameCbAddress = perFrameCbs->GetGPUVirtualAddress();
    for (uint32_t i = 0; i < framesToSimulate; i++)
    {
        Update(context, std::min(simulatedRate, numSecondsSim), perFrameCbAddress, i != lastFrame);
        perFrameCbAddress += sizeof(AlignedPerFrameCb);
        numSecondsSim -= simulatedRate;

        //TODO: Investigate this further and/or report the issue
        // There is (what seems to be) a bug with the debug layer's tracking of implicit resource state tracking that we're triggering here
        // For some reason it becomes convinced that the output buffer's UAV counter was implicitly promoted to D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE
        // This makes zero sense since we aren't using any DXR stuff and my understanding is things don't implicitly promot to be acceleration structures anyway.
        // Flushing the context (IE: executing command lists) clears the issue since that clears out the implicit state promotions.
        if (DebugLayer::IsEnabled())
        { context.Flush(); }
    }

    PIXEndEvent(&context);
    GpuSyncPoint graphicsSyncPoint = context.Finish();

    // Wait for the simulation to complete on the GPU so we can dispose of the temporary per-frame constant buffers
    graphicsSyncPoint.Wait();
}

void ParticleSystem::Reset(ComputeContext& context)
{
    UavCounter& currentCounter = m_ParticleStateBuffers[m_CurrentParticleStateBuffer].Counter;
    context.TransitionResource(currentCounter, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    context.ClearUav(currentCounter);
}
