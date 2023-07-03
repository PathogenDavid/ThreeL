#pragma once
#include "MathCommon.h"
#include <cmath>

struct float4
{
    float x;
    float y;
    float z;
    float w;

    float4()
        : x(), y(), z(), w()
    {
    }

    float4(float x, float y, float z, float w)
        : x(x), y(y), z(z), w(w)
    {
    }

    float4(float s)
        : x(s), y(s), z(s), w(s)
    {
    }

    //---------------------------------------------------------------------------------------------
    // Vector math
    //---------------------------------------------------------------------------------------------
    inline float Dot(float4 other)
    {
        return (x * other.x) + (y * other.y) + (z * other.z) + (w * other.w);
    }

    inline float LengthSquared()
    {
        return Dot(*this);
    }

    inline float Length()
    {
        return std::sqrt(LengthSquared());
    }

    inline float4 Normalized();

    //---------------------------------------------------------------------------------------------
    // Basic vectors
    //---------------------------------------------------------------------------------------------
    static const float4 UnitX;
    static const float4 UnitY;
    static const float4 UnitZ;
    static const float4 UnitW;
    static const float4 One;
    static const float4 Zero;
};

//=====================================================================================================================
// float3 operators
//=====================================================================================================================

//-------------------------------------------------------------------------------------------------
// Unary operators
//-------------------------------------------------------------------------------------------------
inline float4 operator +(float4 v) { return v; }
inline float4 operator -(float4 v) { return float4(-v.x, -v.y, -v.z, -v.w); }

//-------------------------------------------------------------------------------------------------
// Vector arithmetic
//-------------------------------------------------------------------------------------------------
inline float4 operator +(float4 a, float4 b) { return float4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
inline float4 operator -(float4 a, float4 b) { return float4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }
inline float4 operator *(float4 a, float4 b) { return float4(a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w); }
inline float4 operator /(float4 a, float4 b) { return float4(a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w); }

//-------------------------------------------------------------------------------------------------
// Scalar operators
//-------------------------------------------------------------------------------------------------
inline float4 operator +(float4 v, float s) { return float4(v.x + s, v.y + s, v.z + s, v.w + s); }
inline float4 operator -(float4 v, float s) { return float4(v.x - s, v.y - s, v.z - s, v.w - s); }
inline float4 operator *(float4 v, float s) { return float4(v.x * s, v.y * s, v.z * s, v.w * s); }
inline float4 operator /(float4 v, float s) { return float4(v.x / s, v.y / s, v.z / s, v.w / s); }
inline float4 operator +(float s, float4 v) { return float4(s + v.x, s + v.y, s + v.z, s + v.w); }
inline float4 operator -(float s, float4 v) { return float4(s - v.x, s - v.y, s - v.z, s - v.w); }
inline float4 operator *(float s, float4 v) { return float4(s * v.x, s * v.y, s * v.z, s * v.w); }
inline float4 operator /(float s, float4 v) { return float4(s / v.x, s / v.y, s / v.z, s / v.w); }

//-------------------------------------------------------------------------------------------------
// Inline implementations requiring operators
//-------------------------------------------------------------------------------------------------
inline float4 float4::Normalized()
{
    return *this / Length();
}
