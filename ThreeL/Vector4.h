#pragma once
#include "MathCommon.h"
#include "Vector3.h"
#include <cmath>

struct float4
{
    float x;
    float y;
    float z;
    float w;

    float4()
        : x(), y(), z(), w()
    { }

    float4(float x, float y, float z, float w)
        : x(x), y(y), z(z), w(w)
    { }

    float4(float s)
        : x(s), y(s), z(s), w(s)
    { }

    float4(float3 xyz, float w)
        : x(xyz.x), y(xyz.y), z(xyz.z), w(w)
    { }

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

struct uint4
{
    using uint = uint32_t;
    uint x;
    uint y;
    uint z;
    uint w;

    uint4()
        : x(), y(), z(), w()
    {
    }

    uint4(uint x, uint y, uint z, uint w)
        : x(x), y(y), z(z), w(w)
    {
    }

    uint4(uint s)
        : x(s), y(s), z(s), w(s)
    {
    }

    //---------------------------------------------------------------------------------------------
    // Basic vectors
    //---------------------------------------------------------------------------------------------
    static const uint4 UnitX;
    static const uint4 UnitY;
    static const uint4 UnitZ;
    static const uint4 UnitW;
    static const uint4 One;
    static const uint4 Zero;
};

//=====================================================================================================================
// float4 operators
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

//-------------------------------------------------------------------------------------------------
// Per-component math
//-------------------------------------------------------------------------------------------------
namespace Math
{
    inline float4 Clamp(float4 x, float4 min, float4 max)
    {
        return float4
        (
            Clamp(x.x, min.x, max.x),
            Clamp(x.y, min.y, max.y),
            Clamp(x.z, min.z, max.z),
            Clamp(x.w, min.w, max.w)
        );
    }
}
