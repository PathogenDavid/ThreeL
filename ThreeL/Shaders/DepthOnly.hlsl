#include "Common.hlsli"

//===================================================================================================================================================
// Vertex data
//===================================================================================================================================================

struct VsInput
{
    uint VertexId : SV_VertexID;
    float3 Position : POSITION;
    float2 Uv0 : TEXCOORD0;

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
    float4 Color : COLOR;
    float2 Uv0 : TEXCOORD0;
};

//===================================================================================================================================================
// Vertex shader
//===================================================================================================================================================

[RootSignature(PBR_ROOT_SIGNATURE)]
PsInput VsMain(VsInput input)
{
    PsInput result;

    result.Position = float4(input.Position.xyz, 1.f);
    result.Position = mul(result.Position, g_PerNode.Transform);
    result.Position = mul(result.Position, g_PerFrame.ViewProjectionTransform);

    result.Color = input.Color();

    result.Uv0 = input.Uv0;

    return result;
}

//===================================================================================================================================================
// Pixel shader
//===================================================================================================================================================

[RootSignature(PBR_ROOT_SIGNATURE)]
void PsMain(PsInput input)
{
    MaterialParams material = g_Materials[g_PerNode.MaterialId];

    // Get base color
    float4 baseColor = input.Color * material.BaseColorFactor;

    if (material.BaseColorTexture != DISABLED_BUFFER)
    { baseColor *= g_Textures[material.BaseColorTexture].Sample(g_Samplers[material.BaseColorTextureSampler], input.Uv0); }

    // Only base color affects alpha so alpha cutoff can be applied at this point
    if (baseColor.a < material.AlphaCutoff)
    { discard; }
}
