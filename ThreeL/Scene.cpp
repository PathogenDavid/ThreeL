#include "pch.h"
#include "Scene.h"

#include "GltfLoadContext.h"

// glTF uses right-handed y-up coordinates but we use left-handed z-up so we need to convert form one to the other (which is effectively just swapping Y and Z.)
//TODO: Should probably just do this upon mesh load
static const float4x4 g_CoordinateSpaceConversion
(
    1.f, 0.f, 0.f, 0.f,
    0.f, 0.f, 1.f, 0.f,
    0.f, 1.f, 0.f, 0.f,
    0.f, 0.f, 0.f, 1.f
);

Scene::Scene(ResourceManager& resources, const tinygltf::Model& model)
{
    GltfLoadContext context(resources.Graphics, resources, model, *this);
    const tinygltf::Scene& scene = model.scenes[model.defaultScene];

    // Allocate buffers
    std::vector<size_t> meshStarts(model.meshes.size(), -1);
    {
        size_t nextPrimitiveStart = 0;
        size_t i = 0;
        for (const tinygltf::Mesh& mesh : model.meshes)
        {
            meshStarts[i] = nextPrimitiveStart;
            nextPrimitiveStart += mesh.primitives.size();
            i++;
        }

        m_Primitives = std::span(new MeshPrimitive[nextPrimitiveStart], nextPrimitiveStart);
        m_SceneNodes = std::span(new SceneNode[model.nodes.size()], model.nodes.size());
    }

    // Load the scene
    auto GetOrLoadMesh = [&](int meshIndex) -> std::span<MeshPrimitive>
        {
            const tinygltf::Mesh& mesh = model.meshes[meshIndex];

            size_t meshStart = meshStarts[meshIndex];
            std::span<MeshPrimitive> primitives = std::span(&m_Primitives[meshStart], mesh.primitives.size());

            // Load each primitive in the mesh if it isn't already loaded
            if (!primitives[0].IsValid())
            {
                int i = 0;
                for (const tinygltf::Primitive& primitive : mesh.primitives)
                {
                    primitives[i] = MeshPrimitive(context, meshIndex, i);
                    i++;
                }
            }

            return primitives;
        };

    std::function<void(int, const float4x4&)> LoadNode = [&](int nodeIndex, const float4x4& parentTransform) -> void
        {
            // This is not allowed per the glTF spec
            // https://github.com/KhronosGroup/glTF/blob/5de957b8b0a13c147c90d4ff569250440931872f/specification/2.0/README.md#nodes-and-hierarchy
            Assert(!m_SceneNodes[nodeIndex].IsValid() && "Node appears in scene tree more than once!");

            const tinygltf::Node& node = model.nodes[nodeIndex];

            // Build transform
            float4x4 localTransform = float4x4::Identity;

            if (node.matrix.size() == 16)
            {
                localTransform = float4x4
                (
                    (float)node.matrix[0], (float)node.matrix[1], (float)node.matrix[2], (float)node.matrix[3],
                    (float)node.matrix[4], (float)node.matrix[5], (float)node.matrix[6], (float)node.matrix[7],
                    (float)node.matrix[8], (float)node.matrix[9], (float)node.matrix[10], (float)node.matrix[11],
                    (float)node.matrix[12], (float)node.matrix[13], (float)node.matrix[14], (float)node.matrix[15]
                );
            }
            else if (node.scale.size() != 0 || node.rotation.size() != 0 || node.translation.size() != 0)
            {
                Assert(node.matrix.size() == 0);

                float3 scale = float3::One;
                Quaternion rotation = Quaternion::Identity;
                float3 translation = float3::Zero;

                if (node.scale.size() != 0)
                {
                    Assert(node.scale.size() == 3);
                    scale = float3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]);
                }

                if (node.rotation.size() != 0)
                {
                    Assert(node.rotation.size() == 4);
                    rotation = Quaternion((float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2], (float)node.rotation[3]);
                }

                if (node.translation.size() != 0)
                {
                    Assert(node.translation.size() == 3);
                    translation = float3((float)node.translation[0], (float)node.translation[1], (float)node.translation[2]);
                }

                localTransform = float4x4::MakeWorldTransform(translation, scale, rotation);
            }

            // Apply parent transform
            // (We flatten the scene hierarchy for the sake of simplicity since we don't support animation.)
            float4x4 modelLocalTransform = localTransform * parentTransform;

            // Create the mesh and node if it's visible
            if (node.mesh >= 0)
            {
                std::span<MeshPrimitive> meshPrimitives = GetOrLoadMesh(node.mesh);
                std::string nodeName = std::format("{}#{}", node.name, nodeIndex);
                m_SceneNodes[nodeIndex] = SceneNode(nodeName, g_CoordinateSpaceConversion * modelLocalTransform, meshPrimitives);
            }

            // Recursively create the node's childern
            for (int childNodeIndex : node.children)
            {
                LoadNode(childNodeIndex, modelLocalTransform);
            }
        };

    for (int nodeIndex : scene.nodes)
    {
        LoadNode(nodeIndex, float4x4::Identity);
    }
}

Scene::~Scene()
{
    delete[] m_SceneNodes.data();
    delete[] m_Primitives.data();
}
