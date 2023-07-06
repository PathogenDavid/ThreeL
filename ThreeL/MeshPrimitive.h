#pragma once
#include "PbrMaterial.h"

#include <d3d12.h>
#include <string>

class GltfLoadContext;

class MeshPrimitive
{
private:
    std::string m_Name;
    bool m_IsValid = false;

    uint32_t m_VertexOrIndexCount = 0;
    bool m_IsIndexed = false;

    D3D12_INDEX_BUFFER_VIEW m_Indices = { };
    D3D12_VERTEX_BUFFER_VIEW m_Positions = { };
    D3D12_VERTEX_BUFFER_VIEW m_Normals = { };
    D3D12_VERTEX_BUFFER_VIEW m_Uvs = { };
    uint32_t m_ColorsBufferIndex = BUFFER_DISABLED;
    uint32_t m_TangentsBufferIndex = BUFFER_DISABLED;

    PbrMaterial m_Material;

public:
    MeshPrimitive()
    {
    }

    MeshPrimitive(GltfLoadContext& context, int meshIndex, int primitiveIndex);

    inline std::string Name() const { return m_Name; }
    inline bool IsValid() const { return m_IsValid; }
    inline uint32_t VertexOrIndexCount() const { return m_VertexOrIndexCount; }
    inline bool IsIndexed() const { return m_IsIndexed; }
    inline const D3D12_INDEX_BUFFER_VIEW& Indices() const { return m_Indices; }
    inline const D3D12_VERTEX_BUFFER_VIEW& Positions() const { return m_Positions; }
    inline const D3D12_VERTEX_BUFFER_VIEW& Normals() const { return m_Normals; }
    inline const D3D12_VERTEX_BUFFER_VIEW& Uvs() const { return m_Uvs; }
    inline uint32_t ColorsBufferIndex() const { return m_ColorsBufferIndex; }
    inline uint32_t TangentsBufferIndex() const { return m_TangentsBufferIndex; }
    inline const PbrMaterial& Material() const { return m_Material; }
};
