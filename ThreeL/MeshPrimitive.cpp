#include "pch.h"
#include "MeshPrimitive.h"

#include "GltfAccessorView.h"
#include "GltfLoadContext.h"
#include "ResourceManager.h"

#include <tiny_gltf.h>

MeshPrimitive::MeshPrimitive(GltfLoadContext& context, int meshIndex, int primitiveIndex)
    : m_IsValid(true)
{
    const tinygltf::Model& model = context.Model();
    ResourceManager& resources = context.Resources();
    const tinygltf::Mesh& mesh = model.meshes[meshIndex];
    const tinygltf::Primitive& primitive = mesh.primitives[primitiveIndex];

    m_Name = mesh.name.length() > 0 ? mesh.name : "UnnamedMesh";
    m_Name = std::format("{}#{}p{}", m_Name, meshIndex, primitiveIndex);

    // We don't support non-triangle meshes
    Assert(primitive.mode == TINYGLTF_MODE_TRIANGLES && "Only triangle list mode is supported!");

    // Load the index buffer if we have one
    int indexCount = 0;
    if (primitive.indices >= 0)
    {
        m_IsIndexed = true;
        const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
        GltfAccessorViewBase* accessorView = GltfAccessorViewBase::Create(model, indexAccessor);
        indexCount = accessorView->ElementCount();

        // Indices can be in u8, u16, or u32
        // https://github.com/KhronosGroup/glTF/blob/5de957b8b0a13c147c90d4ff569250440931872f/specification/2.0/README.md#primitiveindices
        if (auto indicesU8 = dynamic_cast<GltfAccessorView<uint8_t>*>(accessorView))
        {
            // D3D12 doesn't support 8-bit indices so we don't either, as such we widen them to 16-bit
            // (Realistically we don't expect real models to use these, but some of the test models do and it's called for in the spec.)
            std::vector<uint16_t> indices;
            indices.reserve(indicesU8->ElementCount());

            for (int i = 0; i < indicesU8->ElementCount(); i++)
            {
                indices[i] = (*indicesU8)[i];
            }

            m_Indices = resources.MeshHeap.AllocateIndexBuffer(indices);
        }
        else if (auto indicesU16 = dynamic_cast<GltfAccessorView<uint16_t>*>(accessorView))
        {
            m_Indices = resources.MeshHeap.AllocateIndexBuffer(indicesU16->AsDenseSpanMaybeAllocate());
        }
        else if (auto indicesU32 = dynamic_cast<GltfAccessorView<uint32_t>*>(accessorView))
        {
            m_Indices = resources.MeshHeap.AllocateIndexBuffer(indicesU32->AsDenseSpanMaybeAllocate());
        }
        else
        {
            Assert(false && "Invalid glTF accessor type for mesh primitive indices!");
        }

        delete accessorView;
    }

    // Load vertex data
    bool hasTangents = false;
    {
        GltfAccessorView<float3>* positions = context.GetAttribute<float3>(primitive, "POSITION");
        GltfAccessorView<float3>* normals = context.GetAttribute<float3>(primitive, "NORMAL");
        GltfAccessorView<float4>* tangents = context.GetAttribute<float4>(primitive, "TANGENT");
        GltfAccessorView<float4>* colors0 = context.GetAttribute<float4>(primitive, "COLOR_0");
        GltfAccessorView<float2>* uv0 = context.GetAttribute<float2>(primitive, "TEXCOORD_0");

        //TODO: Read in COLOR_0 if present. (We don't currently have an elegant way to handle missing attributes and none of the test models we care about use it.)
        Assert(!primitive.attributes.contains("COLOR_0") && "Vertex colors are not supported!");

        // Requiring these is non-conformant, but in practice real models will have all of them
        Assert(positions != nullptr && "Position-less primitives are not supported!");
        Assert(normals != nullptr && "Normal-less primitives are not supported!");
        Assert(uv0 != nullptr && "UV-less primitives are not supported!");

        Assert(normals->ElementCount() == positions->ElementCount());
        Assert(uv0->ElementCount() == positions->ElementCount());

        m_Positions = resources.MeshHeap.AllocateVertexBuffer(positions->AsDenseSpanMaybeAllocate());
        m_Normals = resources.MeshHeap.AllocateVertexBuffer(normals->AsDenseSpanMaybeAllocate());
        m_Uvs = resources.MeshHeap.AllocateVertexBuffer(uv0->AsDenseSpanMaybeAllocate());
        m_VertexOrIndexCount = m_IsIndexed ? indexCount : positions->ElementCount();

        if (tangents != nullptr)
        {
            resources.MeshHeap.AllocateVertexBuffer(tangents->AsDenseSpanMaybeAllocate(), &m_TangentsBufferIndex);
            hasTangents = true;
        }
        else
        { m_TangentsBufferIndex = BUFFER_DISABLED; }

        if (colors0 != nullptr)
        { resources.MeshHeap.AllocateVertexBuffer(tangents->AsDenseSpanMaybeAllocate(), &m_ColorsBufferIndex); }
        else
        { m_ColorsBufferIndex = BUFFER_DISABLED; }

        delete positions;
        delete normals;
        delete tangents;
        delete colors0;
        delete uv0;
    }

    // Create material and load textures
    m_Material = PbrMaterial(context, primitive.material, hasTangents);

    // Warn about transparent materials not being implemented
    // (Sponza doesn't use any so I never got around to implementing sorting and rendeirng them.)
    if (m_Material.IsTransparent())
    { printf("Warning: Mesh primitive '%s' will not be rendered because it uses alpha blending, which is currently unimplemented in ThreeL.\n", m_Name.c_str()); }
}
