#include "Common.hlsli"

struct LightLinkedListStatsParams
{
    uint LightLinkedListBufferLength;
};

ConstantBuffer<LightLinkedListStatsParams> g_Params : register(b0, space900);

RWByteAddressBuffer g_LightLinksHeapCounter : register(u0, space900); // Writable just to avoid transitioning the resource

RWByteAddressBuffer g_Results : register(u1, space900);
// [0] u32 Number of light links used this frame
// [4] u32 Max light count per pixel

#define ROOT_SIGNATURE \
    "RootConstants(num32BitConstants = 1, b0, space = 900)," \
    "DescriptorTable(UAV(u0, space = 900))," \
    "UAV(u1, space = 900)," \
    "SRV(t2, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE)," \
    "SRV(t3, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE)," \
    ""

#define GROUP_SIZE 1024

// Sized for worse case scenario of WaveGetLaneCount, in practice most of this will go unused
groupshared uint gs_MaxLightCountInWave[GROUP_SIZE / 4];

[numthreads(GROUP_SIZE, 1, 1)]
[RootSignature(ROOT_SIGNATURE)]
void Main(uint3 dispatchThreadId : SV_DispatchThreadID, uint3 groupThreadId : SV_GroupThreadID)
{
    [branch]
    if (dispatchThreadId.x == 0)
    { g_Results.Store(0, g_LightLinksHeapCounter.Load(0)); }

    [branch]
    if (dispatchThreadId.x >= g_Params.LightLinkedListBufferLength)
    { return; }

    uint lightLinkIndex = g_FirstLightLink.Load(dispatchThreadId.x * 4) & NO_LIGHT_LINK;
    uint lightCount = 0;
    while (lightLinkIndex != NO_LIGHT_LINK)
    {
        lightCount++;
        LightLink lightLink = g_LightLinksHeap[lightLinkIndex];
        lightLinkIndex = lightLink.NextLightIndex();
    }

    // Find the maximum light count in the wave
    uint maxLightCount = WaveActiveMax(lightCount);

    // The first lane in the wave will record the maximum in groupshared memory
    [branch]
    if (WaveIsFirstLane())
    { gs_MaxLightCountInWave[groupThreadId.x / WaveGetLaneCount()] = maxLightCount; }

    // Wait for all threads in the group to record their count
    GroupMemoryBarrierWithGroupSync();

    // The first thread in the group will find the maximum within the group and record it in the results
    [branch]
    if (groupThreadId.x == 0)
    {
        for (uint i = 0; i < GROUP_SIZE / WaveGetLaneCount(); i++)
        { maxLightCount = max(maxLightCount, gs_MaxLightCountInWave[i]); }

        g_Results.InterlockedMax(4, maxLightCount);
    }
}
