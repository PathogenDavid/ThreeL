#include "pch.h"
#include "CameraController.h"

void CameraController::ApplyMovement(float2 moveVector, float2 lookVector)
{
    m_Pitch = Math::Clamp(m_Pitch + lookVector.y, -Math::HalfPi, Math::HalfPi);

    m_Yaw -= Math::Clamp(lookVector.x, -Math::Pi, Math::Pi);
    if (m_Yaw < -Math::Pi)
    { m_Yaw += Math::TwoPi; }
    else if (m_Yaw > Math::Pi)
    { m_Yaw -= Math::TwoPi; }

    // Cap movement speed (mostly to defend against frame spikes yeeting the camera into the void)
    if (moveVector.Length() > 10.f)
    { moveVector = moveVector.Normalized() * 10.f; }

    Quaternion rotation = Quaternion(float3::UnitX, m_Pitch) * Quaternion(float3::UnitY, m_Yaw);
    m_Position = m_Position + float3(moveVector.x, 0.f, moveVector.y) * rotation;
    m_ViewTransform = float4x4::MakeTranslation(-m_Position) * float4x4::MakeRotation(rotation);
}

void CameraController::WarpTo(float3 position, float pitch, float yaw)
{
    m_Position = position;
    m_Pitch = pitch;
    Assert(yaw >= -Math::Pi && yaw <= Math::Pi);
    m_Yaw = Math::Clamp(yaw, -Math::Pi, Math::Pi);

    // Apply a dummy movement to refresh the view transform
    ApplyMovement(float2::Zero, float2::Zero);
}
