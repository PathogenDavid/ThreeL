#pragma once

struct float4x4;

struct float3x3
{
    float m00;
    float m01;
    float m02;
private:
    // Padding is here so this struct matches the HLSL constant buffer layout
    // (It's technically not 100% accurate since HLSL will opportunistically place a 32-bit scalar in the final padding if possible.)
    float padding0;
public:
    float m10;
    float m11;
    float m12;
private:
    float padding1;
public:
    float m20;
    float m21;
    float m22;
private:
    float padding2;
public:
    float3x3
    (
        float m00, float m01, float m02,
        float m10, float m11, float m12,
        float m20, float m21, float m22
    )
        :
        m00(m00), m10(m10), m20(m20), padding0(0.f),
        m01(m01), m11(m11), m21(m21), padding1(0.f),
        m02(m02), m12(m12), m22(m22), padding2(0.f)
    {
    }

    float3x3(float s)
        :
        m00(s), m10(s), m20(s), padding0(0.f),
        m01(s), m11(s), m21(s), padding1(0.f),
        m02(s), m12(s), m22(s), padding2(0.f)
    {
    }

    float3x3()
        : float3x3(0.f)
    {
    }

    static float3x3 MakeInverseTranspose(const float4x4& m);

    inline float3x3& operator *=(float s)
    {
        m00 *= s;
        m01 *= s;
        m02 *= s;
        m10 *= s;
        m11 *= s;
        m12 *= s;
        m20 *= s;
        m21 *= s;
        m22 *= s;
        return *this;
    }

    static const float3x3 Identity;
    static const float3x3 One;
    static const float3x3 Zero;
};

inline float3x3 operator*(float3x3 m, float s) { return m *= s; }
inline float3x3 operator*(float s, float3x3 m) { return m *= s; }
