#pragma once
#include "Assert.h"
#include "MathCommon.h"
#include "Vector3.h"

#include <float.h>

struct Quaternion
{
    float3 Vector;
    float Scalar;

private:
    Quaternion(float w, float3 xyz)
    {
        Vector = xyz;
        Scalar = w;
    }

public:
    Quaternion(float x, float y, float z, float w)
        : Vector(x, y, z), Scalar(w)
    {
    }

    Quaternion(float3 axis, float angle)
    {
        Assert(std::abs(axis.LengthSquared() - 1.f) < FLT_EPSILON && "The axis should be pre-normalized!");
        float halfAngle = angle * 0.5f;
        Vector = std::sin(halfAngle) * axis;
        Scalar = std::cos(halfAngle);
    }

    inline Quaternion Inverse()
    {
        return Quaternion(Scalar, -Vector);
    }

    inline Quaternion operator *(Quaternion b)
    {
        Quaternion a = *this;
        float newScalar = a.Scalar * b.Scalar - a.Vector.Dot(b.Vector);
        float3 newVector = b.Scalar * a.Vector + a.Scalar * b.Vector + a.Vector.Cross(b.Vector);
        return Quaternion(newScalar, newVector);
    }

    inline float3 RotateVector(float3 v)
    {
        return (Inverse() * Quaternion(0.f, v) * *this).Vector;
    }

    static const Quaternion Identity;
};

inline float3 operator *(Quaternion a, float3 b)
{
    return a.RotateVector(b);
}

inline float3 operator *(float3 b, Quaternion a)
{
    return a.RotateVector(b);
}
