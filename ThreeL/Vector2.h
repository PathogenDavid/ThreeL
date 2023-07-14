#pragma once
#include "Assert.h"
#include "MathCommon.h"

#include <cmath>
#include <stdint.h>

struct float2
{
    float x;
    float y;

    float2()
        : x(), y()
    {
    }

    float2(float x, float y)
        : x(x), y(y)
    {
    }

    float2(float s)
        : x(s), y(s)
    {
    }

    //---------------------------------------------------------------------------------------------
    // Vector math
    //---------------------------------------------------------------------------------------------
    inline float Dot(float2 other)
    {
        return (x * other.x) + (y * other.y);
    }

    inline float LengthSquared()
    {
        return Dot(*this);
    }

    inline float Length()
    {
        return std::sqrt(LengthSquared());
    }

    inline float2 Normalized();

    //---------------------------------------------------------------------------------------------
    // Basic vectors
    //---------------------------------------------------------------------------------------------
    static const float2 UnitX;
    static const float2 UnitY;
    static const float2 One;
    static const float2 Zero;
};

struct int2
{
    int x;
    int y;

    int2()
        : x(), y()
    {
    }

    int2(int x, int y)
        : x(x), y(y)
    {
    }

    int2(int s)
        : x(s), y(s)
    {
    }

    //---------------------------------------------------------------------------------------------
    // Conversion operators
    //---------------------------------------------------------------------------------------------
    explicit operator float2() const { return float2((float)x, (float)y); }

    //---------------------------------------------------------------------------------------------
    // Basic vectors
    //---------------------------------------------------------------------------------------------
    static const int2 UnitX;
    static const int2 UnitY;
    static const int2 One;
    static const int2 Zero;
};

struct uint2
{
    using uint = uint32_t;
    uint x;
    uint y;

    constexpr uint2()
        : x(), y()
    {
    }

    constexpr uint2(uint x, uint y)
        : x(x), y(y)
    {
    }

    constexpr uint2(uint s)
        : x(s), y(s)
    {
    }

    //---------------------------------------------------------------------------------------------
    // Conversion operators
    //---------------------------------------------------------------------------------------------
    explicit operator float2() const { return float2((float)x, (float)y); }

    //---------------------------------------------------------------------------------------------
    // Basic vectors
    //---------------------------------------------------------------------------------------------
    static const uint2 UnitX;
    static const uint2 UnitY;
    static const uint2 One;
    static const uint2 Zero;
};

struct bool2
{
    bool x;
    bool y;

    bool2()
        : x(), y()
    {
    }

    bool2(bool x, bool y)
        : x(x), y(y)
    {
    }

    bool2(bool s)
        : x(s), y(s)
    {
    }

    //---------------------------------------------------------------------------------------------
    // All, Any, and None
    //---------------------------------------------------------------------------------------------
    bool All()
    {
        return x && y;
    }


    bool Any()
    {
        return x || y;
    }

    bool None()
    {
        return !Any();
    }
};

//=====================================================================================================================
// uint2 operators
//=====================================================================================================================

//-------------------------------------------------------------------------------------------------
// Boolean operators
//-------------------------------------------------------------------------------------------------
inline bool2 operator==(uint2 a, uint2 b) { return { a.x == b.x, a.y == b.y }; }
inline bool2 operator!=(uint2 a, uint2 b) { return { a.x != b.x, a.y != b.y }; }
inline bool2 operator<=(uint2 a, uint2 b) { return { a.x <= b.x, a.y <= b.y }; }
inline bool2 operator>=(uint2 a, uint2 b) { return { a.x >= b.x, a.y >= b.y }; }
inline bool2 operator<(uint2 a, uint2 b) { return { a.x < b.x, a.y < b.y }; }
inline bool2 operator>(uint2 a, uint2 b) { return { a.x > b.x, a.y > b.y }; }

//-------------------------------------------------------------------------------------------------
// Vector arithmetic
//-------------------------------------------------------------------------------------------------
inline uint2 operator +(uint2 a, uint2 b) { return uint2(a.x + b.x, a.y + b.y); }
inline uint2 operator -(uint2 a, uint2 b) { return uint2(a.x - b.x, a.y - b.y); }
inline uint2 operator *(uint2 a, uint2 b) { return uint2(a.x * b.x, a.y * b.y); }
inline uint2 operator /(uint2 a, uint2 b) { return uint2(a.x / b.x, a.y / b.y); }

//-------------------------------------------------------------------------------------------------
// Scalar operators
//-------------------------------------------------------------------------------------------------
inline uint2 operator +(uint2 v, uint32_t s) { return uint2(v.x + s, v.y + s); }
inline uint2 operator -(uint2 v, uint32_t s) { return uint2(v.x - s, v.y - s); }
inline uint2 operator *(uint2 v, uint32_t s) { return uint2(v.x * s, v.y * s); }
inline uint2 operator /(uint2 v, uint32_t s) { return uint2(v.x / s, v.y / s); }
inline uint2 operator +(uint32_t s, uint2 v) { return uint2(s + v.x, s + v.y); }
inline uint2 operator -(uint32_t s, uint2 v) { return uint2(s - v.x, s - v.y); }
inline uint2 operator *(uint32_t s, uint2 v) { return uint2(s * v.x, s * v.y); }
inline uint2 operator /(uint32_t s, uint2 v) { return uint2(s / v.x, s / v.y); }

//=====================================================================================================================
// float2 operators
//=====================================================================================================================

//-------------------------------------------------------------------------------------------------
// Unary operators
//-------------------------------------------------------------------------------------------------
inline float2 operator +(float2 v) { return v; }
inline float2 operator -(float2 v) { return float2(-v.x, -v.y); }

//-------------------------------------------------------------------------------------------------
// Vector arithmetic
//-------------------------------------------------------------------------------------------------
inline float2 operator +(float2 a, float2 b) { return float2(a.x + b.x, a.y + b.y); }
inline float2 operator -(float2 a, float2 b) { return float2(a.x - b.x, a.y - b.y); }
inline float2 operator *(float2 a, float2 b) { return float2(a.x * b.x, a.y * b.y); }
inline float2 operator /(float2 a, float2 b) { return float2(a.x / b.x, a.y / b.y); }

//-------------------------------------------------------------------------------------------------
// Scalar operators
//-------------------------------------------------------------------------------------------------
inline float2 operator +(float2 v, float s) { return float2(v.x + s, v.y + s); }
inline float2 operator -(float2 v, float s) { return float2(v.x - s, v.y - s); }
inline float2 operator *(float2 v, float s) { return float2(v.x * s, v.y * s); }
inline float2 operator /(float2 v, float s) { return float2(v.x / s, v.y / s); }
inline float2 operator +(float s, float2 v) { return float2(s + v.x, s + v.y); }
inline float2 operator -(float s, float2 v) { return float2(s - v.x, s - v.y); }
inline float2 operator *(float s, float2 v) { return float2(s * v.x, s * v.y); }
inline float2 operator /(float s, float2 v) { return float2(s / v.x, s / v.y); }

//-------------------------------------------------------------------------------------------------
// Inline implementations requiring operators
//-------------------------------------------------------------------------------------------------
inline float2 float2::Normalized()
{
    return *this / Length();
}

//-------------------------------------------------------------------------------------------------
// Per-component math
//-------------------------------------------------------------------------------------------------
namespace Math
{
    inline float2 Clamp(float2 x, float2 min, float2 max)
    {
        return float2
        (
            Clamp(x.x, min.x, max.x),
            Clamp(x.y, min.y, max.y)
        );
    }

    inline uint2 Max(uint2 a, uint2 b)
    {
        return uint2
        (
            std::max(a.x, b.x),
            std::max(a.y, b.y)
        );
    }
}
