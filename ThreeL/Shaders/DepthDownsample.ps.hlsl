#include "FullScreenQuad.vs.hlsl"

#define ROOT_SIGNATURE \
    "DescriptorTable(SRV(t0, numDescriptors = 1, flags = DATA_VOLATILE))," \
    "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_POINT, addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP, addressW = TEXTURE_ADDRESS_CLAMP)," \
    ""

Texture2D<float> g_InputDepthBuffer : register(t0);
SamplerState g_Sampler : register(s0);

// Downscales the input depth buffer to a smaller target depth buffer, keeping only the furthest sample per pixel.
// Note that this shader can only downsample to a target half the size of the input.
// Any unused intermediate buffers *could* be skipped by by doing multiple gathers, but I didn't bother in ThreeL
// for the sake of simplicity and so that the fractional size can be configured at runtime.
[RootSignature(ROOT_SIGNATURE)]
float PsMain(PsInput input) : SV_Depth
{
    float4 depths = g_InputDepthBuffer.GatherRed(g_Sampler, input.Uv);
    return min(depths.x, min(depths.y, min(depths.z, depths.w)));
}
