#include "pch.h"
#include "Matrix4.h"

float float4x4::Determinant() const
{
    return (m00 * m11 - m01 * m10) * (m22 * m33 - m23 * m32)
        - (m00 * m12 - m02 * m10) * (m21 * m33 - m23 * m31)
        + (m00 * m13 - m03 * m10) * (m21 * m32 - m22 * m31)
        + (m01 * m12 - m02 * m11) * (m20 * m33 - m23 * m30)
        - (m01 * m13 - m03 * m11) * (m20 * m32 - m22 * m30)
        + (m02 * m13 - m03 * m12) * (m20 * m31 - m21 * m30);
}

float4x4 float4x4::Inverted() const
{
    float det = Determinant();
    Assert(std::abs(det) > FLT_EPSILON && "Tried to invert non-invertable matrix!");

    det = 1.f / det;
    return float4x4
    (
        det * (m11 * (m22 * m33 - m23 * m32) + m12 * (m23 * m31 - m21 * m33) + m13 * (m21 * m32 - m22 * m31)),
        det * (m21 * (m02 * m33 - m03 * m32) + m22 * (m03 * m31 - m01 * m33) + m23 * (m01 * m32 - m02 * m31)),
        det * (m31 * (m02 * m13 - m03 * m12) + m32 * (m03 * m11 - m01 * m13) + m33 * (m01 * m12 - m02 * m11)),
        det * (m01 * (m13 * m22 - m12 * m23) + m02 * (m11 * m23 - m13 * m21) + m03 * (m12 * m21 - m11 * m22)),
        det * (m12 * (m20 * m33 - m23 * m30) + m13 * (m22 * m30 - m20 * m32) + m10 * (m23 * m32 - m22 * m33)),
        det * (m22 * (m00 * m33 - m03 * m30) + m23 * (m02 * m30 - m00 * m32) + m20 * (m03 * m32 - m02 * m33)),
        det * (m32 * (m00 * m13 - m03 * m10) + m33 * (m02 * m10 - m00 * m12) + m30 * (m03 * m12 - m02 * m13)),
        det * (m02 * (m13 * m20 - m10 * m23) + m03 * (m10 * m22 - m12 * m20) + m00 * (m12 * m23 - m13 * m22)),
        det * (m13 * (m20 * m31 - m21 * m30) + m10 * (m21 * m33 - m23 * m31) + m11 * (m23 * m30 - m20 * m33)),
        det * (m23 * (m00 * m31 - m01 * m30) + m20 * (m01 * m33 - m03 * m31) + m21 * (m03 * m30 - m00 * m33)),
        det * (m33 * (m00 * m11 - m01 * m10) + m30 * (m01 * m13 - m03 * m11) + m31 * (m03 * m10 - m00 * m13)),
        det * (m03 * (m11 * m20 - m10 * m21) + m00 * (m13 * m21 - m11 * m23) + m01 * (m10 * m23 - m13 * m20)),
        det * (m10 * (m22 * m31 - m21 * m32) + m11 * (m20 * m32 - m22 * m30) + m12 * (m21 * m30 - m20 * m31)),
        det * (m20 * (m02 * m31 - m01 * m32) + m21 * (m00 * m32 - m02 * m30) + m22 * (m01 * m30 - m00 * m31)),
        det * (m30 * (m02 * m11 - m01 * m12) + m31 * (m00 * m12 - m02 * m10) + m32 * (m01 * m10 - m00 * m11)),
        det * (m00 * (m11 * m22 - m12 * m21) + m01 * (m12 * m20 - m10 * m22) + m02 * (m10 * m21 - m11 * m20))
    );
}

float4x4 float4x4::MakeRotation(Quaternion q)
{
    float x = q.Vector.x;
    float y = q.Vector.y;
    float z = q.Vector.z;
    float w = q.Scalar;

    float xx2 = x * x * 2.f;
    float yy2 = y * y * 2.f;
    float zz2 = z * z * 2.f;

    float xy2 = x * y * 2.f;
    float xz2 = x * z * 2.f;
    float xw2 = x * w * 2.f;

    float yz2 = y * z * 2.f;
    float yw2 = y * w * 2.f;

    float zw2 = z * w * 2.f;

    return float4x4
    (
        //Row 0
        1.f - yy2 - zz2,
        xy2 + zw2,
        xz2 - yw2,
        0.f,
        //Row 1
        xy2 - zw2,
        1.f - xx2 - zz2,
        yz2 + xw2,
        0.f,
        //Row 2
        xz2 + yw2,
        yz2 - xw2,
        1 - xx2 - yy2,
        0.f,
        //Row 3
        0.f, 0.f, 0.f, 1.f
    );
}

float4x4 float4x4::MakeWorldTransform(float3 position, float3 scale, Quaternion rotation)
{
    //PERF: We could precompute the multiplications here and just generate the matrix outright.
    return MakeScale(scale) * MakeRotation(rotation) * MakeTranslation(position);
}

/// <summary>Creates a standard perspective transform with an infinite far plane and reversed depth.</summary>
/// <remarks>
/// This method generates a perspective transform without an infinite far plane as decribed in
/// "Tightening the Precision of Perspective Rendering" by Paul Upchurch and Mathieu Desburn.
/// https://www.cs.cornell.edu/~paulu/tightening.pdf
///
/// It also uses reversed depth, which improves depth precision where it matters.
/// https://developer.nvidia.com/content/depth-precision-visualized
/// </remarks>
float4x4 float4x4::MakePerspectiveTransformReverseZ(float fieldOfView, float aspect, float nearPlane)
{
    Assert(fieldOfView != 0.f);

    // Equivalent to 1 / tan(fov * 0.5)
    float h = std::tan(Math::HalfPi - fieldOfView * 0.5f);

    return float4x4
    (
        h / aspect, 0.f, 0.f, 0.f,
        0.f, h, 0.f, 0.f,
        0.f, 0.f, 0.f, 1.f,
        0.f, 0.f, nearPlane, 0.f
    );
}

float4x4 float4x4::MakeOrthographicTransform(float width, float height, float nearPlane, float farPlane)
{
    return float4x4
    (
        2.f / width, 0.f, 0.f, 0.f,
        0.f, 2.f / height, 0.f, 0.f,
        0.f, 0.f, 1.f / (farPlane - nearPlane), 0.f,
        0.f, 0.f, (nearPlane / (nearPlane - farPlane)), 1.f
    );
}

float4x4 float4x4::MakeCameraLookAtViewTransform(float3 eye, float3 at, float3 up)
{
    //TODO: When the up vector is parallel to the (at - eye) vector, x will fail because the cross product will be (0, 0, 0).
    // Irrlicht fixes this by offsetting X slightly in CCameraSceneNode::render, not sure how/if we want to handle it here.
    float3 z = (at - eye).Normalized();
    float3 x = up.Cross(z).Normalized();
    float3 y = z.Cross(x);

    return float4x4
    (
        x.x, y.x, z.x, 0.f,
        x.y, y.y, z.y, 0.f,
        x.z, y.z, z.z, 0.f,
        -x.Dot(eye), -y.Dot(eye), -z.Dot(eye), 1.f
    );
}

float4x4 float4x4::Make2dTransform(float width, float height)
{
    return float4x4
    (
        2.f / width, 0.f, 0.f, 0.f,
        0.f, -2.f / height, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        -1.f, 1.f, 0.f, 1.f
    );
}

const float4x4 float4x4::Identity = float4x4
(
    1.f, 0.f, 0.f, 0.f,
    0.f, 1.f, 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f,
    0.f, 0.f, 0.f, 1.f
);

const float4x4 float4x4::One = float4x4(1.f);
const float4x4 float4x4::Zero = float4x4(0.f);

float4x4 operator *(const float4x4& a, const float4x4& b)
{
    return float4x4
    (
        // Row 0
        a.m00 * b.m00 + a.m01 * b.m10 + a.m02 * b.m20 + a.m03 * b.m30,
        a.m00 * b.m01 + a.m01 * b.m11 + a.m02 * b.m21 + a.m03 * b.m31,
        a.m00 * b.m02 + a.m01 * b.m12 + a.m02 * b.m22 + a.m03 * b.m32,
        a.m00 * b.m03 + a.m01 * b.m13 + a.m02 * b.m23 + a.m03 * b.m33,
        // Row 1
        a.m10 * b.m00 + a.m11 * b.m10 + a.m12 * b.m20 + a.m13 * b.m30,
        a.m10 * b.m01 + a.m11 * b.m11 + a.m12 * b.m21 + a.m13 * b.m31,
        a.m10 * b.m02 + a.m11 * b.m12 + a.m12 * b.m22 + a.m13 * b.m32,
        a.m10 * b.m03 + a.m11 * b.m13 + a.m12 * b.m23 + a.m13 * b.m33,
        // Row 2
        a.m20 * b.m00 + a.m21 * b.m10 + a.m22 * b.m20 + a.m23 * b.m30,
        a.m20 * b.m01 + a.m21 * b.m11 + a.m22 * b.m21 + a.m23 * b.m31,
        a.m20 * b.m02 + a.m21 * b.m12 + a.m22 * b.m22 + a.m23 * b.m32,
        a.m20 * b.m03 + a.m21 * b.m13 + a.m22 * b.m23 + a.m23 * b.m33,
        // Row 3
        a.m30 * b.m00 + a.m31 * b.m10 + a.m32 * b.m20 + a.m33 * b.m30,
        a.m30 * b.m01 + a.m31 * b.m11 + a.m32 * b.m21 + a.m33 * b.m31,
        a.m30 * b.m02 + a.m31 * b.m12 + a.m32 * b.m22 + a.m33 * b.m32,
        a.m30 * b.m03 + a.m31 * b.m13 + a.m32 * b.m23 + a.m33 * b.m33
    );
}
