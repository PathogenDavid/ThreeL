#pragma once

//PERF: It'd probably be a good idea to separate this out into things which are intrinsic to the particle
// and things which will change over its lifetime. This particle system is basic enough for it to not
// really matter, but a more complex and flexible system might start to suffer from the excess copying.
struct ParticleState
{
    float3 Velocity;
    float LifeTimer;
    float3 WorldPosition;
    uint MaterialId;
    float Size;
    float Angle;
    float AngularVelocity;
    float4 Color;
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
    sprite.Color = state.Color;

    float cosAngle = cos(state.Angle);
    float sinAngle = sin(state.Angle);
    sprite.Transform = float2x2
    (
        cosAngle, sinAngle,
        -sinAngle, cosAngle
    ) * state.Size;
    return sprite;
}
