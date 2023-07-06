#include "pch.h"
#include "Matrix3.h"

#include "Vector3.h"
#include "Matrix4.h"

float3x3 float3x3::MakeInverseTranspose(const float4x4& m)
{
    float3 x(m.m00, m.m01, m.m02);
    float3 y(m.m10, m.m11, m.m12);
    float3 z(m.m20, m.m21, m.m22);

    float3 inv0 = y.Cross(z);
    float3 inv1 = z.Cross(x);
    float3 inv2 = x.Cross(y);
    float rDet = 1.f / z.Dot(inv2);

    return float3x3(inv0.x, inv0.y, inv0.z, inv1.x, inv1.y, inv1.z, inv2.x, inv2.y, inv2.z) * rDet;
}

const float3x3 float3x3::Identity = float3x3
(
    1.f, 0.f, 0.f,
    0.f, 1.f, 0.f,
    0.f, 0.f, 1.f
);

const float3x3 float3x3::One = float3x3(1.f);
const float3x3 float3x3::Zero = float3x3(0.f);
