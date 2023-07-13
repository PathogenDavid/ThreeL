#pragma once
#include "PbrMaterialHeap.h"

class GltfLoadContext;
class PipelineStateObject;

class PbrMaterial
{
private:
    PipelineStateObject* m_PipelineStateObject = nullptr;
    PbrMaterialId m_MaterialId = -1;
    bool m_IsTransparent = false;
    bool m_IsDoubleSided = false;

public:
    PbrMaterial() = default;
    PbrMaterial(GltfLoadContext& context, int materialIndex, bool primitiveHasTangents);

    inline const PipelineStateObject& PipelineStateObject() const { return *m_PipelineStateObject; }
    inline PbrMaterialId MaterialId() const { return m_MaterialId; }
    inline bool IsTransparent() const { return m_IsTransparent; }
    inline bool IsDoubleSided() const { return m_IsDoubleSided; }
};
