#define DISABLED_BUFFER 0xFFFFFFFF

struct MaterialParams
{
    float AlphaCutoff;
    uint BaseColorTexture;
    uint BaseColorTextureSampler;
    uint MealicRoughnessTexture;
    uint MetalicRoughnessTextureSampler;
    uint NormalTexture;
    uint NormalTextureSampler;
    float NormalTextureScale;

    float4 BaseColorFactor;

    float MetallicFactor;
    float RoughnessFactor;

    uint EmissiveTexture;
    uint EmissiveTextureSampler;
    float3 EmissiveFactor;
};

struct PerNode
{
    float4x4 Transform;
    float3x3 NormalTransform;
    float _padding; // Ensure MatrialId is in its own row, makes CPU side struct simpler
    uint MaterialId;
    uint ColorsIndex;
    uint TangentsIndex;
};

struct PerFrame
{
    float4x4 ViewProjectionTransform;
};

ConstantBuffer<PerNode> g_PerNode : register(b0);
ConstantBuffer<PerFrame> g_PerFrame : register(b1);
StructuredBuffer<MaterialParams> g_Materials : register(t0);

SamplerState g_Samplers[] : register(space1);
Texture2D g_Textures[] : register(space2);
ByteAddressBuffer g_Buffers[] : register(space3);

#define ROOT_SIGNATURE \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
    "RootConstants(num32BitConstants = 31, b0)," \
    "RootConstants(num32BitConstants = 16, b1)," \
    "SRV(t0, flags = DATA_STATIC)," \
    "DescriptorTable(" \
        "Sampler(s0, space = 1, offset = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE)" \
    ")," \
    "DescriptorTable(" \
        "SRV(t0, space = 2, offset = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE | DATA_VOLATILE)," \
        "SRV(t0, space = 3, offset = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE | DATA_VOLATILE)" \
    ")," \
    ""

//===================================================================================================================================================
// Mesh data
//===================================================================================================================================================

struct VsInput
{
    uint VertexId : SV_VertexID;
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 Uv0 : TEXCOORD0;

    float4 TangentWorld()
    {
        [branch]
        if (g_PerNode.TangentsIndex == DISABLED_BUFFER)
            return 0.f.xxxx;

        float4 tangent = g_Buffers[g_PerNode.TangentsIndex].Load<float4>(VertexId * sizeof(float4));
        return float4(normalize(mul(tangent.xyz, g_PerNode.NormalTransform)), tangent.w);
    }

    float4 Color()
    {
        [branch]
        if (g_PerNode.ColorsIndex == DISABLED_BUFFER)
            return 1.f.xxxx;

        return g_Buffers[g_PerNode.ColorsIndex].Load<float4>(VertexId * sizeof(float4));
    }
};

struct PsInput
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float4 Color : COLOR;
    float2 Uv0 : TEXCOORD0;
};

//===================================================================================================================================================
// Entry-points
//===================================================================================================================================================

[RootSignature(ROOT_SIGNATURE)]
PsInput VsMain(VsInput input)
{
    PsInput result;

    result.Position = float4(input.Position.xyz, 1.f);
    result.Position = mul(result.Position, g_PerNode.Transform);
    result.Position = mul(result.Position, g_PerFrame.ViewProjectionTransform);

    result.Normal = normalize(mul(input.Normal, g_PerNode.NormalTransform));
    result.Tangent = input.TangentWorld();

    result.Color = input.Color();

    result.Uv0 = input.Uv0;

    return result;
}

[RootSignature(ROOT_SIGNATURE)]
float4 PsMain(PsInput input, bool isFrontFace: SV_IsFrontFace) : SV_Target
{
    MaterialParams material = g_Materials[g_PerNode.MaterialId];

    // Calculate base color
    float4 baseColor = input.Color * material.BaseColorFactor;

    if (material.BaseColorTexture != DISABLED_BUFFER)
    {
        baseColor *= g_Textures[material.BaseColorTexture].Sample(g_Samplers[material.BaseColorTextureSampler], input.Uv0);
    }

    if (baseColor.a < material.AlphaCutoff)
        discard;

    // Get tangent frame
    float3 normal = normalize(input.Normal);

    if (material.NormalTexture != DISABLED_BUFFER)
    {
        float3 tangent = normalize(input.Tangent.xyz);
        float3 bitangent = cross(normal, tangent) * input.Tangent.w;

        if (!isFrontFace)
        {
            normal *= -1.f;
            tangent *= -1.f;
            bitangent *= -1.f;
        }

        float3x3 tangentFrame = float3x3(tangent, bitangent, normal);

        float3 textureNormal = g_Textures[material.NormalTexture].Sample(g_Samplers[material.NormalTextureSampler], input.Uv0).rgb;
        textureNormal = textureNormal * 2.f - 1.f;
        textureNormal *= float3(material.NormalTextureScale.xx, 1.f);
        textureNormal = normalize(textureNormal);
        normal = mul(textureNormal, tangentFrame);
    }

    float4 result = baseColor;
    result.rgb = (normal + 1.f.xxx) / 2.f.xxx;

    return result;
}
