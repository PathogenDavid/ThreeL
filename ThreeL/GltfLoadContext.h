#pragma once
#include "Assert.h"
#include "SamplerHeap.h"

#include <tiny_gltf.h>

class GraphicsCore;
struct ResourceManager;
class Scene;
class Texture;
class GltfAccessorViewBase;
template<typename T> class GltfAccessorView;

class GltfLoadContext
{
private:
    const GraphicsCore& m_Graphics;
    ResourceManager& m_Resources;
    const tinygltf::Model& m_Model;
    Scene& m_Scene;

public:
    GltfLoadContext(const GraphicsCore& graphics, ResourceManager& resources, const tinygltf::Model& model, Scene& scene)
        : m_Graphics(graphics), m_Resources(resources), m_Model(model), m_Scene(scene)
    {
    }

    inline const GraphicsCore& Graphics() const { return m_Graphics; }
    inline ResourceManager& Resources() const { return m_Resources;  }
    inline const tinygltf::Model& Model() const { return m_Model;  }

    SamplerId LoadSampler(int samplerIndex);
    inline SamplerId LoadSamplerForTexture(int textureIndex)
    {
        return LoadSampler(m_Model.textures[textureIndex].sampler);
    }

    const Texture& LoadTexture(int textureIndex, bool isSrgb);

    GltfAccessorViewBase* GetAttributeRaw(const tinygltf::Primitive& primitive, const std::string& attributeName);

    template<typename T>
    GltfAccessorView<T>* GetAttribute(const tinygltf::Primitive& primitive, const std::string& attributeName)
    {
        GltfAccessorViewBase* result = GetAttributeRaw(primitive, attributeName);

        if (result == nullptr)
        {
            return nullptr;
        }
        else if (auto resultT = dynamic_cast<GltfAccessorView<T>*>(result))
        {
            return resultT;
        }
        else
        {
            Assert(false && "Attribute is of an unexpected type!");
            return nullptr;
        }
    }
};
