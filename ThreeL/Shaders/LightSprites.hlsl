#include "Common.hlsli"

struct PsInput
{
    float4 Position : SV_Position;
    float3 Color : COLOR;
    float2 Uv0 : TEXCOORD0;
};

Texture2D g_Texture : register(t0, space900);
SamplerState g_Sampler : register(s0, space900);

#define ROOT_SIGNATURE \
    "CBV(b1)," \
    "SRV(t1, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE)," \
    "DescriptorTable(SRV(t0, space = 900, flags = DATA_STATIC))," \
    "StaticSampler(s0, space = 900, filter = FILTER_MIN_MAG_MIP_LINEAR, addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP, addressW = TEXTURE_ADDRESS_CLAMP)," \
    ""

//===================================================================================================================================================
// Vertex shader
//===================================================================================================================================================

[RootSignature(ROOT_SIGNATURE)]
PsInput VsMain(uint vertexId : SV_VertexID, uint lightIndex : SV_InstanceID)
{
    PsInput result;
    LightInfo light = g_Lights[lightIndex];

    result.Color = light.Color;
    result.Uv0 = float2(uint2(vertexId >> 1, vertexId) & 1.xx);

    float2 corner = lerp(float2(-1.f, 1.f), float2(1.f, -1.f), result.Uv0) * light.Range * 0.10f;
    float3 cornerOffset = float3(corner, 0.f);
    cornerOffset = mul(cornerOffset, (float3x3)g_PerFrame.ViewTransformInverse);

    result.Position = float4(light.Position + cornerOffset, 1.f);
    result.Position = mul(result.Position, g_PerFrame.ViewProjectionTransform);

    return result;
}

//===================================================================================================================================================
// Pixel shader
//===================================================================================================================================================

[RootSignature(ROOT_SIGNATURE)]
float4 PsMain(PsInput input) : SV_Target
{
    float distance = g_Texture.Sample(g_Sampler, input.Uv0).r;
    if (distance < 0.1f) discard; // Janky threshold because SDF is actually lazy Photoshop gradient stroke
    return float4(input.Color, 1.f);
}
