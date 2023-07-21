#include "Common.hlsli"
#include "FullScreenQuad.vs.hlsl"

enum class LightLinkedListDebugMode : uint
{
    None,
    LightCount,
    AverageLightColor,
    NearestLight,
};

struct LightLinkedListDebugParams
{
    LightLinkedListDebugMode Mode;
    uint MaxLightsPerPixel;
    float DebugOverlayAlpha;
};

ConstantBuffer<LightLinkedListDebugParams> g_Params : register(b0, space900);

#define LLL_DEBUG_ROOT_SIGNATURE \
    "RootConstants(num32BitConstants = 3, b0, space = 900, visibility = SHADER_VISIBILITY_PIXEL)," \
    "CBV(b1, visibility = SHADER_VISIBILITY_PIXEL)," \
    "SRV(t1, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE, visibility = SHADER_VISIBILITY_PIXEL)," \
    "SRV(t2, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE, visibility = SHADER_VISIBILITY_PIXEL)," \
    "SRV(t3, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE, visibility = SHADER_VISIBILITY_PIXEL)," \
    ""

[RootSignature(LLL_DEBUG_ROOT_SIGNATURE)]
float4 PsMain(PsInput input) : SV_Target
{
    uint2 position = (uint2)input.Position.xy;
    uint2 lightLinkedListPosition = ScreenSpaceToLightLinkedListSpace(position);
    uint lightLinkIndex = g_FirstLightLink.Load(GetFirstLightLinkAddress(lightLinkedListPosition)) & NO_LIGHT_LINK;
    
    uint lightCount = 0;
    LightLink closestLight;
    float closestLightMinDepth = 1.#INF;
    float3 averageColor = 0.f.xxx;
    while (lightLinkIndex != NO_LIGHT_LINK)
    {
        lightCount++;
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

    averageColor /= (float)lightCount;

    float3 color = 0.f.xxx;
    switch (g_Params.Mode)
    {
        case LightLinkedListDebugMode::None: // This value exists for the CPU, draws shouldn't be submitted for it.
        case LightLinkedListDebugMode::LightCount:
        {
            float x = (float)lightCount / (float)g_Params.MaxLightsPerPixel;
            color = saturate(float3
            (
                x * 3.f - 2.f,
                x * 3.f,
                2.f - x * 3.f
            ));
            break;
        }
        case LightLinkedListDebugMode::AverageLightColor:
        {
            if (lightCount == 0)
            { discard; }
            else
            { color = averageColor; }
            break;
        }
        case LightLinkedListDebugMode::NearestLight:
        {
            if (lightCount == 0)
            { discard; }
            else
            {
                LightInfo light = g_Lights[closestLight.LightId()];
                color = light.Color;
            }
            break;
        }
    }

    return float4(color, g_Params.DebugOverlayAlpha);
}
