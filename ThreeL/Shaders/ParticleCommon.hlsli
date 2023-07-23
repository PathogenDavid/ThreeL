#pragma once

struct ParticleState
{
    float3 Velocity;
    float LifeTimer;
    float3 WorldPosition;
    uint MaterialId;
};

struct ParticleSprite
{
    float3 WorldPosition;
    uint MaterialId;
    float4 Color;
    float2x2 Transform;
};

ParticleSprite MakeSprite(ParticleState state)
{
    ParticleSprite sprite;
    sprite.WorldPosition = state.WorldPosition;
    sprite.MaterialId = state.MaterialId;
    sprite.Color = 1.f.xxxx;
    sprite.Transform = float2x2(1.f, 0.f, 0.f, 1.f);
    return sprite;
}
