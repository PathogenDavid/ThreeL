#include "ParticleCommon.hlsli"
#include "Common.hlsli"

StructuredBuffer<ParticleState> g_ParticleStatesIn : register(t0, space900);
ByteAddressBuffer g_LivingParticleCount : register(t1, space900);

RWStructuredBuffer<ParticleState> g_ParticleStatesOut : register(u0, space900);
RWStructuredBuffer<ParticleSprite> g_ParticleSpritesOut : register(u1, space900);

RWByteAddressBuffer g_LivingParticleCountOut : register(u2, space900);
RWByteAddressBuffer g_DrawIndirectArguments : register(u3, space900);

struct ParticleSystemParams
{
    uint ParticleCapacity;
    uint ToSpawnThisFrame;
};

ConstantBuffer<ParticleSystemParams> g_Params : register(b0, space900);

#define ROOT_SIGNATURE \
    "RootConstants(num32BitConstants = 2, b0, space = 900)," \
    "CBV(b1)," \
    "SRV(t0, space = 900, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE)," \
    "SRV(t1, space = 900, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE)," \
    "DescriptorTable(UAV(u0, space = 900, flags = DATA_VOLATILE))," \
    "UAV(u1, space = 900, flags = DATA_VOLATILE)," \
    "UAV(u2, space = 900, flags = DATA_VOLATILE)," \
    "UAV(u3, space = 900, flags = DATA_VOLATILE)," \
    ""

void OutputParticle(uint outputIndex, ParticleState state);

[numthreads(64, 1, 1)]
[RootSignature(ROOT_SIGNATURE)]
void MainSpawn(uint3 threadId : SV_DispatchThreadID)
{
    if (threadId.x >= g_Params.ToSpawnThisFrame)
    { return; }

    // Allocate a slot for the new particle
    uint outputIndex = g_ParticleStatesOut.IncrementCounter();

    // Particle system is at capacity, can't spawn more particles
    if (outputIndex >= g_Params.ParticleCapacity)
    { return; }

    ParticleState state;
    state.Velocity = float3(0.f, 5.f, 0.f);
    state.LifeTimer = 1.f;
    state.WorldPosition = float3(0.2f, 0.f, 0.f);
    state.MaterialId = 103; //TODO: Hard-coded material ID
    OutputParticle(outputIndex, state);
}

[numthreads(64, 1, 1)]
[RootSignature(ROOT_SIGNATURE)]
void MainUpdate(uint3 threadId : SV_DispatchThreadID)
{
    uint inputIndex = threadId.x;
    uint livingParticleCount = g_LivingParticleCount.Load(0);

    if (inputIndex >= g_Params.ParticleCapacity || inputIndex >= livingParticleCount)
    { return; }

    ParticleState state = g_ParticleStatesIn[inputIndex];
    state.LifeTimer -= g_PerFrame.DeltaTime;

    // If the particle died there's nothing left to do
    if (state.LifeTimer <= 0.f)
    { return; }

    // Update the particle
    state.WorldPosition += state.Velocity * g_PerFrame.DeltaTime;

    // Write the particle out
    // No need to check if outputIndex is in-bounds since we know we're only processing up to the capacity from the input
    uint outputIndex = g_ParticleStatesOut.IncrementCounter();
    OutputParticle(outputIndex, state);
}

void OutputParticle(uint outputIndex, ParticleState state)
{
    // Save the updated particle state
    g_ParticleStatesOut[outputIndex] = state;

    // Create a sprite for this particle
    //TODO: Emit a sort key
    //TODO: Culling?
    g_ParticleSpritesOut[outputIndex] = MakeSprite(state);
}

[numthreads(1, 1, 1)]
[RootSignature(ROOT_SIGNATURE)]
void MainPrepareDrawIndirect()
{
    // Particle count can be beyond capacity when spawning fails due to the system being full
    uint particleCount = min(g_LivingParticleCountOut.Load(0), g_Params.ParticleCapacity);

    g_DrawIndirectArguments.Store(0, 4); // VertexCountPerInstance
    g_DrawIndirectArguments.Store(4, particleCount); // InstanceCount
    g_DrawIndirectArguments.Store(8, 1); // StartVertexLocation
    g_DrawIndirectArguments.Store(12, 1); // StartInstanceLocation
}
