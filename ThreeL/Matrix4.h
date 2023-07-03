#pragma once
#include "MathCommon.h"
#include "Quaternion.h"
#include "Vector2.h"
#include "Vector3.h"

struct float4x4
{
    float m00;
    float m10;
    float m20;
    float m30;
    float m01;
    float m11;
    float m21;
    float m31;
    float m02;
    float m12;
    float m22;
    float m32;
    float m03;
    float m13;
    float m23;
    float m33;

    float4x4
    (
        float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33
    )
        :
        m00(m00), m10(m10), m20(m20), m30(m30),
        m01(m01), m11(m11), m21(m21), m31(m31),
        m02(m02), m12(m12), m22(m22), m32(m32),
        m03(m03), m13(m13), m23(m23), m33(m33)
    {
    }

    float4x4(float s)
        :
        m00(s), m10(s), m20(s), m30(s),
        m01(s), m11(s), m21(s), m31(s),
        m02(s), m12(s), m22(s), m32(s),
        m03(s), m13(s), m23(s), m33(s)
    {
    }

    float4x4()
        : float4x4(0.f)
    {
    }

    //---------------------------------------------------------------------------------------------
    // Matrix properties
    //---------------------------------------------------------------------------------------------
    inline float4x4 Transposed()
    {
        return float4x4
        (
            m00, m10, m20, m30,
            m01, m11, m21, m31,
            m02, m12, m22, m32,
            m03, m13, m23, m33
        );
    }

    //---------------------------------------------------------------------------------------------
    // Matrix creation
    //---------------------------------------------------------------------------------------------
    static inline float4x4 MakeTranslation(float x, float y, float z)
    {
        return float4x4
        (
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            +x, +y, +z, 1.f
        );
    }

    static inline float4x4 MakeTranslation(float3 v)
    {
        return MakeTranslation(v.x, v.y, v.z);
    }

    static inline float4x4 MakeScale(float x, float y, float z)
    {
        return float4x4
        (
            +x, 0.f, 0.f, 0.f,
            0.f, +y, 0.f, 0.f,
            0.f, 0.f, +z, 0.f,
            0.f, 0.f, 0.f, 1.f
        );
    }

    static inline float4x4 MakeScale(float3 v)
    {
        return MakeScale(v.x, v.y, v.z);
    }

    static inline float4x4 MakeScale(float s)
    {
        return MakeScale(s, s, s);
    }

    static float4x4 MakeRotation(Quaternion q);

    inline float4x4 MakeRotation(float3 axis, float angle)
    {
        return MakeRotation(Quaternion(axis, angle));
    }

    static float4x4 MakeWorldTransform(float3 position, float3 scale, Quaternion rotation);

    /// <summary>Creates a standard perspective transform with an infinite far plane and reversed depth.</summary>
    /// <remarks>
    /// This method generates a perspective transform without an infinite far plane as decribed in
    /// "Tightening the Precision of Perspective Rendering" by Paul Upchurch and Mathieu Desburn.
    /// https://www.cs.cornell.edu/~paulu/tightening.pdf
    ///
    /// It also uses reversed depth, which improves depth precision where it matters.
    /// https://developer.nvidia.com/content/depth-precision-visualized
    /// </remarks>
    static float4x4 MakePerspectiveTransformReverseZ(float fieldOfView, float aspect, float nearPlane);

    static float4x4 MakeOrthographicTransform(float width, float height, float nearPlane, float farPlane);

    static float4x4 MakeCameraLookAtViewTransform(float3 eye, float3 at, float3 up);

    /// <summary>Makes a transform matrix that transforms a pixel coordinate (with 0,0 being the top-left corner) to clip-space.</summary>
    /// <param name="width">The width of the viewport to transform to</param>
    /// <param name="height">The height of the viewport to transform to</param>
    /// <returns>The generated matrix.</returns>
    /// <remarks>The generated matrix is basically an orthographic perspective transform, but it also flips the Y axis and shifts 0,0 to the left-top.</remarks>
    static float4x4 Make2dTransform(float width, float height);

    static inline float4x4 Make2dTransform(float2 viewportSize)
    {
        return Make2dTransform(viewportSize.x, viewportSize.y);
    }

    //---------------------------------------------------------------------------------------------
    // Basic matricies
    //---------------------------------------------------------------------------------------------
    static const float4x4 Identity;
    static const float4x4 One;
    static const float4x4 Zero;
};

//=====================================================================================================================
// float4x4 operators
//=====================================================================================================================

//-------------------------------------------------------------------------------------------------
// Unary operators
//-------------------------------------------------------------------------------------------------

inline float4x4 operator+(const float4x4& m) { return m; }
inline float4x4 operator-(const float4x4& m)
{
    return float4x4
    (
        -m.m00, -m.m01, -m.m02, -m.m03,
        -m.m10, -m.m11, -m.m12, -m.m13,
        -m.m20, -m.m21, -m.m22, -m.m23,
        -m.m30, -m.m31, -m.m32, -m.m33
    );
}

//-------------------------------------------------------------------------------------------------
// Matrix arithmetic
//-------------------------------------------------------------------------------------------------

inline float4x4 operator +(const float4x4& a, const float4x4& b)
{
    return float4x4
    (
        a.m00 + b.m00, a.m01 + b.m01, a.m02 + b.m02, a.m03 + b.m03,
        a.m10 + b.m10, a.m11 + b.m11, a.m12 + b.m12, a.m13 + b.m13,
        a.m20 + b.m20, a.m21 + b.m21, a.m22 + b.m22, a.m23 + b.m23,
        a.m30 + b.m30, a.m31 + b.m31, a.m32 + b.m32, a.m33 + b.m33
    );
}

inline float4x4 operator -(const float4x4& a, const float4x4& b)
{
    return float4x4
    (
        a.m00 - b.m00, a.m01 - b.m01, a.m02 - b.m02, a.m03 - b.m03,
        a.m10 - b.m10, a.m11 - b.m11, a.m12 - b.m12, a.m13 - b.m13,
        a.m20 - b.m20, a.m21 - b.m21, a.m22 - b.m22, a.m23 - b.m23,
        a.m30 - b.m30, a.m31 - b.m31, a.m32 - b.m32, a.m33 - b.m33
    );
}

//-------------------------------------------------------------------------------------------------
// Scalar operations
//-------------------------------------------------------------------------------------------------

inline float4x4 operator *(const float4x4& m, float s)
{
    return float4x4
    (
        m.m00 * s, m.m01 * s, m.m02 * s, m.m03 * s,
        m.m10 * s, m.m11 * s, m.m12 * s, m.m13 * s,
        m.m20 * s, m.m21 * s, m.m22 * s, m.m23 * s,
        m.m30 * s, m.m31 * s, m.m32 * s, m.m33 * s
    );
}

inline float4x4 operator *(float s, const float4x4& m)
{
    return float4x4
    (
        s * m.m00, s * m.m01, s * m.m02, s * m.m03,
        s * m.m10, s * m.m11, s * m.m12, s * m.m13,
        s * m.m20, s * m.m21, s * m.m22, s * m.m23,
        s * m.m30, s * m.m31, s * m.m32, s * m.m33
    );
}

inline float4x4 operator /(const float4x4& m, float s)
{
    return float4x4
    (
        m.m00 / s, m.m01 / s, m.m02 / s, m.m03 / s,
        m.m10 / s, m.m11 / s, m.m12 / s, m.m13 / s,
        m.m20 / s, m.m21 / s, m.m22 / s, m.m23 / s,
        m.m30 / s, m.m31 / s, m.m32 / s, m.m33 / s
    );
}

inline float4x4 operator /(float s, const float4x4& m)
{
    return float4x4
    (
        s / m.m00, s / m.m01, s / m.m02, s / m.m03,
        s / m.m10, s / m.m11, s / m.m12, s / m.m13,
        s / m.m20, s / m.m21, s / m.m22, s / m.m23,
        s / m.m30, s / m.m31, s / m.m32, s / m.m33
    );
}

//-------------------------------------------------------------------------------------------------
// Matrix multiplication
//-------------------------------------------------------------------------------------------------
float4x4 operator *(const float4x4& a, const float4x4& b);
