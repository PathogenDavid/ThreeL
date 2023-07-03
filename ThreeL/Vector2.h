#pragma once
#include "Assert.h"
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
    // Conversions
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

    //---------------------------------------------------------------------------------------------
    // Truthiness operators
    //---------------------------------------------------------------------------------------------
    inline operator bool()
    {
        return All();
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
