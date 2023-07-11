struct GenerateMipmapChainParams
{
    uint2 OutputSize;
    float2 OutputSizeInverse;
    bool OutputIsSrgb;
};

ConstantBuffer<GenerateMipmapChainParams> g_Params : register(b0);
SamplerState g_Sampler : register(s0);
Texture2D g_InputTexture : register(t0);

#ifdef GENERATE_UNORM_MIPMAP_CHAIN
RWTexture2D<unorm float4> g_OutputTexture : register(u0);
#else
RWTexture2D<float4> g_OutputTexture : register(u0);
#endif

#define ROOT_SIGNATURE \
    "RootConstants(num32BitConstants = 5, b0)," \
    "DescriptorTable(SRV(t0, numDescriptors = 1, flags = DATA_VOLATILE))," \
    "DescriptorTable(UAV(u0, numDescriptors = 1, flags = DATA_VOLATILE))," \
    "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR, addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP, addressW = TEXTURE_ADDRESS_CLAMP)," \
    ""

[numthreads(8, 8, 1)]
[RootSignature(ROOT_SIGNATURE)]
void Main(uint3 outputLocation : SV_DispatchThreadID)
{
    if (outputLocation.x >= g_Params.OutputSize.x || outputLocation.y >= g_Params.OutputSize.y)
        return;

    float2 uv = ((float2)outputLocation.xy + 0.5f.xx) * g_Params.OutputSizeInverse;
    float4 rs = g_InputTexture.GatherRed(g_Sampler, uv);
    float4 gs = g_InputTexture.GatherGreen(g_Sampler, uv);
    float4 bs = g_InputTexture.GatherBlue(g_Sampler, uv);
    float4 as = g_InputTexture.GatherAlpha(g_Sampler, uv);

    float4 outputColor = 0.f.xxxx;
    float alphaSum = as.x + as.y + as.z + as.w;

    if (alphaSum > 0.f)
    {
        // Weight input colors by their alpha to avoid having fully transparent texels influencing the output
        outputColor.rgb += float3(rs.x, gs.x, bs.x) * as.x;
        outputColor.rgb += float3(rs.y, gs.y, bs.y) * as.y;
        outputColor.rgb += float3(rs.z, gs.z, bs.z) * as.z;
        outputColor.rgb += float3(rs.w, gs.w, bs.w) * as.w;
        outputColor.rgb /= alphaSum;
        // Use maximum alpha gathered to preserve coverage
        outputColor.a = max(as.x, max(as.y, max(as.z, as.z)));
    }
    else
    {
        // All texels are fully transparent, use a simpler average to preserve colors
        outputColor += float4(rs.x, gs.x, bs.x, as.x);
        outputColor += float4(rs.y, gs.y, bs.y, as.y);
        outputColor += float4(rs.z, gs.z, bs.z, as.z);
        outputColor += float4(rs.w, gs.w, bs.w, as.w);
        outputColor *= 0.25f;
    }

    if (g_Params.OutputIsSrgb)
    {
        // Unlike SRVs, UAVs cannot view sRGB textures so values will be stored as if they were for linear RGB
        // As such we need to manually convert the color back to sRGB before storing so that they're loaded correctly for SRVs
        outputColor.rgb = pow(outputColor.rgb, 1.0f / 2.2f);
    }

    g_OutputTexture[outputLocation.xy] = outputColor;
}
