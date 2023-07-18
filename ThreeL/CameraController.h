#pragma once
#include "Math.h"

class CameraController
{
private:
    float m_Pitch = 0.f;
    float m_Yaw = 0.f;
    float3 m_Position = float3(0.f, 1.f, 0.f);//float3::Zero;
    float4x4 m_ViewTransform = float4x4::Identity;

public:
    void ApplyMovement(float2 moveVector, float2 lookVector);
    void WarpTo(float3 position, float pitch, float yaw);

    inline const float3& Position() const { return m_Position; }
    inline const float Pitch() const { return m_Pitch; }
    inline const float Yaw() const { return m_Yaw; }
    inline const float4x4& ViewTransform() const { return m_ViewTransform; }
};
