#pragma once
#include "Math.h"

#include <span>
#include <string>

class MeshPrimitive;

class SceneNode
{
    std::string m_Name;
    float4x4 m_WorldTransform;
    float3x3 m_NormalTransform;
    std::span<const MeshPrimitive> m_MeshPrimitives;

public:
    SceneNode()
    {
    }

    SceneNode(std::string name, float4x4 worldTransform, std::span<const MeshPrimitive> meshPrimitives)
        : m_Name(name), m_WorldTransform(worldTransform), m_NormalTransform(float3x3::MakeInverseTranspose(worldTransform)), m_MeshPrimitives(meshPrimitives)
    {
    }

    inline bool IsValid() const
    {
        return m_MeshPrimitives.data() != nullptr && m_MeshPrimitives.size() != 0;
    }

    inline std::string Name() const { return m_Name; }
    inline float4x4 WorldTransform() const { return m_WorldTransform; }
    inline float3x3 NormalTransform() const { return m_NormalTransform; }
    inline std::span<const MeshPrimitive> MeshPrimitives() const { return m_MeshPrimitives; }

    inline auto begin() const { return m_MeshPrimitives.begin(); }
    inline auto end() const { return m_MeshPrimitives.end(); }
};
