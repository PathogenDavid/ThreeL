#pragma once
#include "MeshPrimitive.h"
#include "ResourceManager.h"
#include "SceneNode.h"
#include "Texture.h"

#include <tiny_gltf.h>
#include <span>
#include <vector>

class Scene
{
    // GltfLoadContext is responsible for managing our various resource caches
    friend class GltfLoadContext;

private:
    std::span<MeshPrimitive> m_Primitives;
    std::span<SceneNode> m_SceneNodes;

    // (Image index + sRGB bit) -> Texture
public://TODO: TEMP
    std::unordered_map<uint64_t, std::unique_ptr<Texture>> m_TextureCache;

public:
    Scene(ResourceManager& resources, const tinygltf::Model& model);

    inline std::span<const SceneNode> SceneNodes() const { return m_SceneNodes; }
    inline auto begin() const { return SceneNodes().begin(); }
    inline auto end() const { return SceneNodes().end(); }

    ~Scene();
};
