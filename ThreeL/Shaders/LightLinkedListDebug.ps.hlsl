#include "Common.hlsli"
#include "FullScreenQuad.vs.hlsl"

#define LLL_DEBUG_ROOT_SIGNATURE \
    "CBV(b1)," \
    "SRV(t1, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE)," \
    "SRV(t2, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE, visibility = SHADER_VISIBILITY_PIXEL)," \
    "SRV(t3, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE, visibility = SHADER_VISIBILITY_PIXEL)," \
    ""

[RootSignature(LLL_DEBUG_ROOT_SIGNATURE)]
float4 PsMain(PsInput input) : SV_Target
{
    uint2 position = (uint2)input.Position.xy;
    uint2 lightLinkedListPosition = ScreenSpaceToLightLinkedListSpace(position);
    uint lightLinkIndex = g_FirstLightLink.Load(GetFirstLightLinkAddress(lightLinkedListPosition)) & NO_LIGHT_LINK;
    
    uint count = 0;
    LightLink closestLight;
    float closestLightMinDepth = 999999999.f;
    float3 averageColor = 0.f.xxx;
    while (lightLinkIndex != NO_LIGHT_LINK)
    {
        count++;
        LightLink lightLink = g_LightLinksHeap[lightLinkIndex];
        lightLinkIndex = lightLink.NextLightIndex();

        averageColor += g_Lights[lightLink.LightId()].Color;

        float minDepth = lightLink.MinDepth();
        if (minDepth < closestLightMinDepth)
        {
            closestLight = lightLink;
            closestLightMinDepth = minDepth;
        }
    }

    averageColor /= (float)count;

#if !true
    if (count == 0)
    {
        discard;
        return 0.f.xxxx;
    }
    else
    {
        return float4((float)count, 1.f, (float)lastLightLink.LightId(), 1.f);
    }
#elif false
    switch (count)
    {
        case 0: return float4(0.f, 0.f, 1.f, 0.7f);
        case 1:
            switch (lastLightLink.LightId())
            {
                case 0: return float4(1.f, 0.f, 0.f, 0.7f);
                case 1: return float4(1.f, 0.f, 1.f, 0.7f);
                default: return float4(0.f, 1.f, 0.f, 0.7f);
            }
            //return float4(0.f, 1.f, 0.f, 0.7f);
        case 2: return float4(0.f, 1.f, 1.f, 0.7f);
        default: return float4(1.f, 0.f, 0.f, 0.7f);
    }
#else
    if (count == 0)
    {
        discard;
        return 0.f.xxxx;
    }

    //LightInfo light = g_Lights[closestLight.LightId()];
    //return float4(light.Color, 0.7f);
    return float4(averageColor, 0.25f);
#endif
}
