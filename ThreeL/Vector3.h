#pragma once
#include "MathCommon.h"
#include "Vector2.h"
#include <cmath>

struct float3
{
    float x;
    float y;
    float z;

    float3()
        : x(), y(), z()
    {
    }

    float3(float x, float y, float z)
        : x(x), y(y), z(z)
    {
    }

    float3(float s)
        : x(s), y(s), z(s)
    {
    }

    //---------------------------------------------------------------------------------------------
    // Vector math
    //---------------------------------------------------------------------------------------------
    inline float Dot(float3 other)
    {
        return (x * other.x) + (y * other.y) + (z * other.z);
    }

    inline float3 Cross(float3 other)
    {
        return float3(y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x);
    }

    inline float LengthSquared()
    {
        return Dot(*this);
    }

    inline float Length()
    {
        return std::sqrt(LengthSquared());
    }

    inline float3 Normalized();

    //---------------------------------------------------------------------------------------------
    // Basic vectors
    //---------------------------------------------------------------------------------------------
    static const float3 UnitX;
    static const float3 UnitY;
    static const float3 UnitZ;
    static const float3 One;
    static const float3 Zero;
};

struct uint3
{
    using uint = uint32_t;
    uint x;
    uint y;
    uint z;

    constexpr uint3() : x(), y(), z() { }
    constexpr uint3(uint x, uint y, uint z) : x(x), y(y), z(z) { }
    constexpr uint3(uint2 xy, uint z) : x(xy.x), y(xy.y), z(z) { }
    constexpr uint3(uint x, uint2 yz) : x(x), y(yz.x), z(yz.y) { }
    constexpr uint3(uint s) : x(s), y(s), z(s) { }

    //---------------------------------------------------------------------------------------------
    // Conversion operators
    //---------------------------------------------------------------------------------------------
    explicit operator float3() const { return float3((float)x, (float)y, (float)z); }

    //---------------------------------------------------------------------------------------------
    // Basic vectors
    //---------------------------------------------------------------------------------------------
    static const uint3 UnitX;
    static const uint3 UnitY;
    static const uint3 UnitZ;
    static const uint3 One;
    static const uint3 Zero;
};

//=====================================================================================================================
// float3 operators
//=====================================================================================================================

//-------------------------------------------------------------------------------------------------
// Unary operators
//-------------------------------------------------------------------------------------------------
inline float3 operator +(float3 v) { return v; }
inline float3 operator -(float3 v) { return float3(-v.x, -v.y, -v.z); }

//-------------------------------------------------------------------------------------------------
// Vector arithmetic
//-------------------------------------------------------------------------------------------------
inline float3 operator +(float3 a, float3 b) { return float3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline float3 operator -(float3 a, float3 b) { return float3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline float3 operator *(float3 a, float3 b) { return float3(a.x * b.x, a.y * b.y, a.z * b.z); }
inline float3 operator /(float3 a, float3 b) { return float3(a.x / b.x, a.y / b.y, a.z / b.z); }

//-------------------------------------------------------------------------------------------------
// Scalar operators
//-------------------------------------------------------------------------------------------------
inline float3 operator +(float3 v, float s) { return float3(v.x + s, v.y + s, v.z + s); }
inline float3 operator -(float3 v, float s) { return float3(v.x - s, v.y - s, v.z - s); }
inline float3 operator *(float3 v, float s) { return float3(v.x * s, v.y * s, v.z * s); }
inline float3 operator /(float3 v, float s) { return float3(v.x / s, v.y / s, v.z / s); }
inline float3 operator +(float s, float3 v) { return float3(s + v.x, s + v.y, s + v.z); }
inline float3 operator -(float s, float3 v) { return float3(s - v.x, s - v.y, s - v.z); }
inline float3 operator *(float s, float3 v) { return float3(s * v.x, s * v.y, s * v.z); }
inline float3 operator /(float s, float3 v) { return float3(s / v.x, s / v.y, s / v.z); }

//-------------------------------------------------------------------------------------------------
// Inline implementations requiring operators
//-------------------------------------------------------------------------------------------------
inline float3 float3::Normalized()
{
    return *this / Length();
}

//-------------------------------------------------------------------------------------------------
// Per-component math
//-------------------------------------------------------------------------------------------------
namespace Math
{
    inline float3 Clamp(float3 x, float3 min, float3 max)
    {
        return float3
        (
            Clamp(x.x, min.x, max.x),
            Clamp(x.y, min.y, max.y),
            Clamp(x.z, min.z, max.z)
        );
    }
}
