#pragma once
#include <numbers>

namespace Math
{
    const float Pi = (float)std::numbers::pi;
    const float TwoPi = 2.f * Pi;
    const float HalfPi = 0.5f * Pi;
    const float QuarterPi = 0.25f * Pi;
    const float InversePi = 1.f / Pi;

    inline float Clamp(float x, float min, float max)
    {
        return x < min ? min : (x > max ? max : x);
    }

    inline float Deg2Rad(float x)
    {
        return x / 180.f * Pi;
    }

    inline float Rad2Deg(float x)
    {
        return x / Pi * 180.f;
    }

    inline float Frac(float x)
    {
        float _unused;
        return std::modf(x, &_unused);
    }

    inline uint32_t DivRoundUp(uint32_t numerator, uint32_t denominator)
    {
        return (numerator + denominator - 1) / denominator;
    }
}
