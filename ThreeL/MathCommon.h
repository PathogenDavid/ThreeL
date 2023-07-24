#pragma once
#include <numbers>

namespace Math
{
    const float Pi = (float)std::numbers::pi;
    const float TwoPi = 2.f * Pi;
    const float HalfPi = 0.5f * Pi;
    const float QuarterPi = 0.25f * Pi;
    const float InversePi = 1.f / Pi;

    inline constexpr float Clamp(float x, float min, float max)
    {
        return x < min ? min : (x > max ? max : x);
    }

    inline constexpr float Deg2Rad(float x)
    {
        return x / 180.f * Pi;
    }

    inline constexpr float Rad2Deg(float x)
    {
        return x / Pi * 180.f;
    }

    inline float Frac(float x)
    {
        float _unused;
        return std::modf(x, &_unused);
    }

    inline constexpr uint32_t DivRoundUp(uint32_t numerator, uint32_t denominator)
    {
        return (numerator + denominator - 1) / denominator;
    }

    inline uint8_t Log2(uint64_t x)
    {
        DWORD mostSignificantBit;
        DWORD leastSignificantBit;

        if (_BitScanReverse64(&mostSignificantBit, x) && _BitScanForward64(&leastSignificantBit, x))
        {
            uint8_t result = (uint8_t)mostSignificantBit;

            // If x is not a perfect power of two (IE: multiple bits set) we round up to the next power of two
            if (mostSignificantBit != leastSignificantBit)
            { result++; }

            return result;
        }

        return 0;
    }

    template<typename T>
    inline T AlignPowerOfTwo(T x)
    {
        return x == 0 ? 0 : 1 << Log2(x);
    }

    inline bool IsPowerOfTwo(uint32_t x)
    {
        return x && !(x & (x - 1u));
    }
}
