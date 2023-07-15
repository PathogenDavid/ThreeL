#include "Common.hlsli"

//===================================================================================================================================================
// Vertex data
//===================================================================================================================================================

struct VsInput
{
    float3 Position : POSITION;
    uint LightIndex : SV_InstanceID;
};

struct PsInput
{
    float4 Position : SV_Position;
    uint LightIndex : LIGHTINDEX;
    float ExtraScale : EXTRASCALE;
};

//===================================================================================================================================================
// Vertex shader
//===================================================================================================================================================

[RootSignature(LLL_FILL_ROOT_SIGNATURE)]
PsInput VsMain(VsInput input)
{
    LightInfo light = g_Lights[input.LightIndex];
    PsInput result;

    // Grow the light slightly to ensure it covers the center of pixels we're filling
    //TODO: Actually calculate this value
    float extraScale = 1.f;

    result.Position = float4(light.Position + input.Position * light.Range * extraScale, 1.f);
    result.Position = mul(result.Position, g_PerFrame.ViewProjectionTransform);

    result.LightIndex = input.LightIndex;

    return result;
}

//===================================================================================================================================================
// Pixel shader
//===================================================================================================================================================

[RootSignature(LLL_FILL_ROOT_SIGNATURE)]
void PsMain(PsInput input)
{
    LightInfo light = g_Lights[input.LightIndex];

    //TODO: This is temporary just to make sure the basics are all working
    g_FirstLightLinkRW.Store(GetFirstLightLinkAddress((uint2)input.Position.xy), input.LightIndex);
}
