#include "Common.hlsli"
#include "FullScreenQuad.vs.hlsl"

#define LLL_DEBUG_ROOT_SIGNATURE \
    "CBV(b1)," \
    "SRV(t1, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE)," \
    "UAV(u1, space = 900, flags = DATA_VOLATILE)," \
    ""

[RootSignature(LLL_DEBUG_ROOT_SIGNATURE)]
float4 PsMain(PsInput input) : SV_Target
{
    //TODO: This is temporary for testing, the real implementation needs to talk the linked list and count up the links
    uint2 position = (uint2)input.Position.xy;
    uint2 lightLinkedListPosition = ScreenSpaceToLightLinkedListSpace(position);
    uint lightIndex = g_FirstLightLinkRW.Load(GetFirstLightLinkAddress(lightLinkedListPosition));

    if (lightIndex > 256)
    {
        discard;
        return 0.f.xxxx;
    }

    LightInfo light = g_Lights[lightIndex];
    return float4(light.Color, 0.01f);
}
