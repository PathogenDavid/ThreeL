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
    float4x4 NormalTransform; //TODO: Should ideally be float3x3, we just don't have a float3x3 type on the CPU side
    uint MaterialId;
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

#define ROOT_SIGNATURE \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
    "RootConstants(num32BitConstants = 33, b0)," \
    "RootConstants(num32BitConstants = 16, b1)," \
    "SRV(t0, flags = DATA_STATIC)," \
    "DescriptorTable(" \
        "Sampler(s0, space = 1, offset = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE)" \
    ")," \
    "DescriptorTable(" \
        "SRV(t0, space = 2, offset = 0, numDescriptors = unbounded, flags = DESCRIPTORS_VOLATILE | DATA_VOLATILE)" \
    ")," \
    ""

//===================================================================================================================================================
// Mesh data
//===================================================================================================================================================

struct VsInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    //float4 Color : COLOR; // None of the models we care about use vertex colors so we ignore them.
    float2 Uv0 : TEXCOORD0;
};

struct PsInput
{
    float4 Position : SV_Position;
    float3 Normal : NORMAL;
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

    result.Normal = mul(float4(input.Normal, 0.f), g_PerNode.NormalTransform).xyz;

    result.Color = (1.f).xxxx;//input.Color;

    result.Uv0 = input.Uv0;

    return result;
}

[RootSignature(ROOT_SIGNATURE)]
float4 PsMain(PsInput input) : SV_Target
{
    MaterialParams material = g_Materials[g_PerNode.MaterialId];

    float lightIntensity = 0.2f;
    const float3 lightRay = normalize(-float3(0.5f, 0.2f, -1.f));
    lightIntensity += max(0.f, dot(lightRay, input.Normal)) * 0.4f;
    lightIntensity = min(lightIntensity, 1.f);

    float4 result = g_Textures[material.BaseColorTexture].Sample(g_Samplers[material.BaseColorTextureSampler], input.Uv0);// * input.Color;
    //result.rgb *= lightIntensity;
    
    if (result.a < material.AlphaCutoff)
        discard;

    return result;
}
