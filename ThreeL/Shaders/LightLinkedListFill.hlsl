#include "Common.hlsli"

struct LightLinkedListFillParams
{
    uint LightLinksLimit;
};

ConstantBuffer<LightLinkedListFillParams> g_FillParams : register(b0, space900);
Texture2D<float> g_DepthBuffer : register(t0, space900);
RWStructuredBuffer<LightLink> g_LightLinksHeapRW : register(u0, space900);
RWByteAddressBuffer g_FirstLightLinkRW : register(u1, space900);

#define LLL_FILL_ROOT_SIGNATURE \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," \
    "RootConstants(num32BitConstants = 1, b0, space = 900)," \
    "CBV(b1)," \
    "SRV(t1, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE)," \
    "DescriptorTable(" \
        "SRV(t0, space = 900, flags = DATA_STATIC_WHILE_SET_AT_EXECUTE)," \
        "visibility = SHADER_VISIBILITY_PIXEL" \
    ")," \
    "DescriptorTable(" \
        "UAV(u0, space = 900, flags = DATA_VOLATILE)," \
        "visibility = SHADER_VISIBILITY_PIXEL" \
    ")," \
    "UAV(u1, space = 900, flags = DATA_VOLATILE)," \
    ""

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
    // noperspective is needed here as these represent points on planes parallel to the near plane rather than the triangle,
    // so we want them interpolated in screen space -- IE: without the perspective correction.
    // (See https://www.david-colson.com/2021/11/30/ps1-style-renderer.html#warped-textures for details on what this does.)
    noperspective float3 RayPlane0 : RAYPLANE0;
    noperspective float3 RayPlane1 : RAYPLANE1;
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
    result.ExtraScale = extraScale;

    // Calculate the ray planes
    // In order to figure out the ray from the eye to our sphere, we unproject our clip space position onto two planes parallel to near plane in world space.
    // The point on the near plane gives us the ray origin, the difference between the two points gives us the ray's direction.
    // (Actually getting a point on the origin exactly doesn't matter for us since we treat the ray as a line.)
    // We calculate those two points here for use in the pixel shader, which greatly reduces the number of matrix multiplies needed there.
    float2 clipPosition = result.Position.xy / result.Position.w;
    float4 rayPlane0 = mul(float4(clipPosition, 1.f, 1.f), g_PerFrame.ViewProjectionTransformInverse);
    float4 rayPlane1 = mul(float4(clipPosition, 0.0001f, 1.f), g_PerFrame.ViewProjectionTransformInverse);
    result.RayPlane0 = rayPlane0.xyz / rayPlane0.w;
    result.RayPlane1 = rayPlane1.xyz / rayPlane1.w;

    return result;
}

//===================================================================================================================================================
// Pixel shader
//===================================================================================================================================================

[RootSignature(LLL_FILL_ROOT_SIGNATURE)]
void PsMain(PsInput input)
{
    LightInfo light = g_Lights[input.LightIndex];

    float3 rayOrigin = input.RayPlane0;
    float3 rayDirection = normalize(input.RayPlane1 - input.RayPlane0);

    // Find intersections between ray and sphere
    // (Note that our ray is actually treated as a line so it doesn't matter if the eye is inside the light)
    // https://raytracing.github.io/books/RayTracingInOneWeekend.html#addingasphere/ray-sphereintersection
    float3 point1;
    float3 point2;
    {
        float radius = light.Range * input.ExtraScale;
        float3 delta = rayOrigin - light.Position;
        float a0 = dot(delta, delta) - radius * radius;
        float a1 = dot(rayDirection, delta);
        float discriminant = a1 * a1 - a0;

        if (discriminant <= 0.f)
        {
            // Ray is either outside of (or exactly tangent to) the sphere
            // This will inevitably happen because our light mesh is not a perfect sphere
            return;
        }

        float root = sqrt(discriminant);
        float t0 = -a1 - root;
        float t1 = -a1 + root;

        point1 = rayOrigin + rayDirection * t0;
        point2 = rayOrigin + rayDirection * t1;
    }

    // Transform the point into clip space
    // (DXC will eliminate all the unecessary multiplies for the unused components.)
    float4 transformed1 = mul(float4(point1, 1.f), g_PerFrame.ViewProjectionTransform);

    // Skip light if front is occluded
    // (perspective depth will be negative if we're inside the light)
    float depth1 = transformed1.z / transformed1.w;
    if (depth1 >= 0 && depth1 < g_DepthBuffer[(uint2)input.Position.xy])
    { return; }

    // Allocate a light link for this pixel
    uint lightLinkIndex = g_LightLinksHeapRW.IncrementCounter();

    if (lightLinkIndex >= g_FillParams.LightLinksLimit)
    {
        // We overflowed the light links heap, can't continue. Light buffer will be incomplete.
        return;
    }

    // Store this index as this pixel's first light index, keeping the old value as our new next index
    uint nextLightLinkIndex;
    g_FirstLightLinkRW.InterlockedExchange(GetFirstLightLinkAddress((uint2)input.Position.xy), lightLinkIndex, nextLightLinkIndex);
    nextLightLinkIndex &= NO_LIGHT_LINK;

    // Build and store our light link
    // Using linear 16-bit float for depth is not super ideal, but is probably good enough for this case
    // https://therealmjp.github.io/posts/attack-of-the-depth-buffer/
    float4 transformed2 = mul(float4(point2, 1.f), g_PerFrame.ViewProjectionTransform);
    LightLink lightLink;
    lightLink.SetDepths(transformed1.w, transformed2.w);
    lightLink.SetIds(input.LightIndex, nextLightLinkIndex);
    g_LightLinksHeapRW[lightLinkIndex] = lightLink;
}
