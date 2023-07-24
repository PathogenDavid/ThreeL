#include "ParticleCommon.hlsli"
#include "Common.hlsli"
#include "Random.hlsli"

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
    float MaxSize;
    float FadeOutTime;

    float LifeMin;
    float LifeMax;
    float AngularVelocityMin;
    float AngularVelocityMax;

    float3 VelocityDirectionVariance;
    float VelocityMagnitudeMin;

    float3 VelocityDirectionBias;
    float VelocityMagnitudeMax;

    float3 SpawnPoint;
    uint MinMaterialId;

    float3 SpawnPointVariance;
    uint MaxMaterialId;

    float3 BaseColor;
    float MinShade;

    float MaxShade;
};

ConstantBuffer<ParticleSystemParams> g_Params : register(b0, space900);

#define ROOT_SIGNATURE \
    "RootConstants(num32BitConstants = 29, b0, space = 900)," \
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

    Random random;
    random.Init(Hash(uint2(g_PerFrame.FrameNumber, threadId.x)));

    ParticleState state;
    state.LifeTimer = random.NextFloat(g_Params.LifeMin, g_Params.LifeMax);

    state.Velocity = normalize
    (
        random.NextFloat3(-g_Params.VelocityDirectionVariance, g_Params.VelocityDirectionVariance)
        + g_Params.VelocityDirectionBias
    ) * random.NextFloat(g_Params.VelocityMagnitudeMin, g_Params.VelocityMagnitudeMax);

    state.WorldPosition = g_Params.SpawnPoint + random.NextFloat3(-g_Params.SpawnPointVariance, g_Params.SpawnPointVariance);

    state.MaterialId = random.NextUint(g_Params.MinMaterialId, g_Params.MaxMaterialId + 1);
    state.Size = 0.f;
    state.Angle = random.NextFloat(-Math::Pi, Math::Pi);
    state.AngularVelocity = random.NextFloat(g_Params.AngularVelocityMin, g_Params.AngularVelocityMax);
    state.Color = float4(g_Params.BaseColor * random.NextFloat(g_Params.MinShade, g_Params.MaxShade), 1.f);
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

    float deltaTime = g_PerFrame.DeltaTime;
    ParticleState state = g_ParticleStatesIn[inputIndex];
    state.LifeTimer -= deltaTime;

    // If the particle died there's nothing left to do
    if (state.LifeTimer <= 0.f)
    { return; }

    // Update the particle
    state.WorldPosition += state.Velocity * deltaTime;
    if (state.Size < g_Params.MaxSize)
    {
        state.Size = min(g_Params.MaxSize, state.Size + (g_Params.MaxSize - state.Size) * 0.5f * deltaTime);
    }
    state.Angle += state.AngularVelocity * deltaTime;

    if (state.LifeTimer < g_Params.FadeOutTime)
    {
        state.Color.a = smoothstep(0.f, 1.f, state.LifeTimer / g_Params.FadeOutTime);
    }

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

    uint4 arguments = uint4
    (
        4, // VertexCountPerInstance
        particleCount, // InstanceCount
        1, // StartVertexLocation
        1 // StartInstanceLocation
    );
    g_DrawIndirectArguments.Store4(0, arguments);
}
