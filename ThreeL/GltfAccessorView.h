#pragma once
#include <tiny_gltf.h>

class GltfAccessorViewBase
{
protected:
    const tinygltf::Model& m_Model;
    const tinygltf::Accessor& m_Accessor;

    GltfAccessorViewBase(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
        : m_Model(model), m_Accessor(accessor)
    {
    }

public:
    inline int ElementCount() const
    {
        return (int)m_Accessor.count;
    }

    static GltfAccessorViewBase* Create(const tinygltf::Model& model, const tinygltf::Accessor& accessor);

    virtual ~GltfAccessorViewBase() = default;
};

template<typename TElement>
class GltfAccessorView : public GltfAccessorViewBase
{
protected:
    GltfAccessorView(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
        : GltfAccessorViewBase(model, accessor)
    {
    }

public:
    virtual const TElement& operator[](size_t index) const = 0;

    virtual std::span<const TElement> AsDenseSpanMaybeAllocate() = 0;
};
