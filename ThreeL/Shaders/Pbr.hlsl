#include "Common.hlsli"

//===================================================================================================================================================
// Vertex data
//===================================================================================================================================================

struct VsInput
{
    uint VertexId : SV_VertexID;
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 Uv0 : TEXCOORD0;

    float4 TangentWorld()
    {
        [branch]
        if (g_PerNode.TangentsIndex == DISABLED_BUFFER)
            return 0.f.xxxx;

        float4 tangent = g_Buffers[g_PerNode.TangentsIndex].Load<float4>(VertexId * sizeof(float4));
        return float4(normalize(mul(tangent.xyz, g_PerNode.NormalTransform)), tangent.w);
    }

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
    float3 WorldPosition : WORLDPOSITION;
    float3 Normal : NORMAL;
    float4 Tangent : TANGENT;
    float4 Color : COLOR;
    float2 Uv0 : TEXCOORD0;
#ifdef PBR_IS_PARTICLE // Material comes from vertex shader for particles since they're instanced (see ParticleRender.hlsl)
    uint MaterialId : MATERIALID;
#endif
};

//===================================================================================================================================================
// Vertex shader
//===================================================================================================================================================

[RootSignature(PBR_ROOT_SIGNATURE)]
PsInput VsMain(VsInput input)
{
    PsInput result;

    result.Position = float4(input.Position, 1.f);
    result.Position = mul(result.Position, g_PerNode.Transform);
    result.WorldPosition = result.Position.xyz;
    result.Position = mul(result.Position, g_PerFrame.ViewProjectionTransform);

    result.Normal = normalize(mul(input.Normal, g_PerNode.NormalTransform));
    result.Tangent = input.TangentWorld();

    result.Color = input.Color();

    result.Uv0 = input.Uv0;

    return result;
}

//===================================================================================================================================================
// Pixel shader
//===================================================================================================================================================

float clampedDot(float3 a, float3 b)
{
    return clamp(dot(a, b), 0.f, 1.f);
}

// The BRDF implementation here is based on the techniques recommended by the glTF specificaiton
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#appendix-b-brdf-implementation
// (Note that the math will not match 1:1 until you reach the simplifications described in the final section.)
struct Brdf
{
    // Surface-dependent fields
    float3 BaseColor;
    float RoughnessAlpha;
    float3 WorldPosition;
    float3 Normal; // N
    float3 ViewDirection; // V
    float NdotV;
    float3 DiffuseColor;
    float3 F0;
    float3 F90;

    // Outputs
    float3 Diffuse;
    float3 Specular;

    // Light-dependent temporaries
    float3 SurfaceToLight; // L
    float3 HalfVector; // H
    float NdotL;
    float NdotH;
    float VdotH;

    float3 Frensel_Schlick()
    {
        float vhPart = 1.f - VdotH;
        float vh2 = vhPart * vhPart;
        float vh5 = vh2 * vh2 * vhPart;

        return F0 + (F90 - F0) * vh5;
    }

    float3 DiffuseBrdf_Lambertian()
    {
        return DiffuseColor / Math::Pi;
    }

    // Specular BRDF using GGX microfacet distribution
    // https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#specular-brdf
    float SpecularBrdf_GGX()
    {
        float alphaSquared = RoughnessAlpha * RoughnessAlpha;

        // Calculate visibility function
        // Using the derived simplificaiton described here:
        // https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf/geometricshadowing(specularg)
        float visibility;
        {
            visibility = NdotL * sqrt(NdotV * NdotV * (1.f - alphaSquared) + alphaSquared);
            visibility += NdotV * sqrt(NdotL * NdotL * (1.f - alphaSquared) + alphaSquared);
            visibility = visibility > 0.f ? 0.5f / visibility : 0.f;
        }

        // Calculate Trowbridge-Reitz microfacet distribution
        float distribution;
        {
            distribution = NdotH * NdotH * (alphaSquared - 1.f) + 1.f;
            distribution = alphaSquared / (Math::Pi * distribution * distribution);
        }

        return visibility * distribution;
    }

    void ApplyLight(float3 surfaceToLight, float3 lightColor, float lightIntensity)
    {
        SurfaceToLight = surfaceToLight;
        HalfVector = normalize(SurfaceToLight + ViewDirection);
        NdotL = dot(Normal, SurfaceToLight);

#ifdef PBR_IS_PARTICLE
        // For particles only we do an extreme approximation of light scattering within the particle
        // Right now we don't incorporate a BTDF (IE: transmission of light through objects isn't simulated) so we have to pretend light shines through
        // Additionally from a math perspective our particles are flat quads, but from a visual perspective they're 3D clouds of smoke so we need to simulate that
        // Doing this isn't physically accurate, but it's good enough to make our particles lit in a way that makes sense
        if (NdotL < 0.f)
        { Normal = -Normal; }
        NdotL = 1.f;
#endif

        NdotH = clampedDot(Normal, HalfVector);
        VdotH = clampedDot(ViewDirection, HalfVector);

#ifndef PBR_IS_PARTICLE
        // Surface is not hit by the light
        if (NdotL <= 0.f)
        { return; }
#endif

        float3 frensel = Frensel_Schlick();
        Diffuse += lightIntensity * lightColor * NdotL * (1.f.xxx - frensel) * DiffuseBrdf_Lambertian();
        Specular += lightIntensity * lightColor * NdotL * frensel * SpecularBrdf_GGX();
    }

    void ApplyDirectionalLight(float3 lightDirection, float3 lightColor, float lightIntensity)
    {
        ApplyLight(-lightDirection, lightColor, lightIntensity);
    }

    void ApplyPointLight(LightInfo light)
    {
        float3 surfaceToLight = light.Position - WorldPosition;

        // Calculate attenuation based on the KHR_lights_punctual spec with minimum bounds on denominator to avoid explosion at light center
        // https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_lights_punctual/README.md#range-property
        float distanceSquared = dot(surfaceToLight, surfaceToLight);
        float rangeSquared = light.Range * light.Range;
        float distanceOverRange = distanceSquared / rangeSquared; // pow2
        distanceOverRange *= distanceOverRange; // pow4
        float lightAttenuation = saturate(1.f - distanceOverRange) / max(distanceSquared, 0.0001);

        // When this is enabled it's easier to see the boundaries of light spheres, which are normally pretty hard to see since the light
        // attenuation is approaching zero at that point. Basically this shows dim solid light for areas covered by the light linked list
        // and bright in areas actually covered by the light. It also indirectly shows light checks lost due to depth errors.
        // In other words, this shows that RangeExtension in LightLinkedListFill.hlsl is being calculated correctly.
#if DEBUG_LIGHT_BOUNDARIES
        lightAttenuation = distanceSquared < rangeSquared ? 1.f : 0.25f;
        Diffuse += light.Color * lightAttenuation;
#else
        // Surface is outside the range of the light
        if (lightAttenuation <= 0.f)
        { return; }

        ApplyLight(surfaceToLight, light.Color, light.Intensity * lightAttenuation);
#endif
    }
};

[RootSignature(PBR_ROOT_SIGNATURE)]
float4 PsMain(PsInput input, bool isFrontFace: SV_IsFrontFace) : SV_Target
{
    //=================================================================================================================
    // Read in material attributes
    //=================================================================================================================
#ifdef PBR_IS_PARTICLE
    MaterialParams material = g_Materials[input.MaterialId];
#else
    MaterialParams material = g_Materials[g_PerNode.MaterialId];
#endif

    // Get base color
    float4 baseColor = input.Color * material.BaseColorFactor;

    if (material.BaseColorTexture != DISABLED_BUFFER)
    { baseColor *= g_Textures[material.BaseColorTexture].Sample(g_Samplers[material.BaseColorTextureSampler], input.Uv0); }

    // Only base color affects alpha so alpha cutoff can be applied at this point
    if (baseColor.a < material.AlphaCutoff)
    { discard; }

    // Get metalness/roughness
    float metalness = material.MetallicFactor;
    float roughness = material.RoughnessFactor;

    if (material.MealicRoughnessTexture != DISABLED_BUFFER)
    {
        float4 mr = g_Textures[material.MealicRoughnessTexture].Sample(g_Samplers[material.MetalicRoughnessTextureSampler], input.Uv0);
        metalness *= mr.b;
        roughness *= mr.g;
    }

    // Get tangent frame
    float3 normal = normalize(input.Normal);

    if (!isFrontFace)
    { normal *= -1.f; }

    if (material.NormalTexture != DISABLED_BUFFER)
    {
        float3 tangent = normalize(input.Tangent.xyz);
        float3 bitangent = cross(tangent, normal) * input.Tangent.w;

        if (!isFrontFace)
        {
            tangent *= -1.f;
            bitangent *= -1.f;
        }

        float3x3 tangentFrame = float3x3(tangent, bitangent, normal);

        float3 textureNormal = g_Textures[material.NormalTexture].Sample(g_Samplers[material.NormalTextureSampler], input.Uv0).rgb;
        textureNormal = textureNormal * 2.f - 1.f;
        textureNormal *= float3(material.NormalTextureScale.xx, 1.f);
        textureNormal = normalize(textureNormal);
        normal = mul(textureNormal, tangentFrame);
    }

    // Get emissive color
    float3 emissive = material.EmissiveFactor;

    if (material.EmissiveTexture != DISABLED_BUFFER)
    { emissive *= g_Textures[material.EmissiveTexture].Sample(g_Samplers[material.EmissiveTextureSampler], input.Uv0).rgb; }

    //=================================================================================================================
    // PBR calculations
    //=================================================================================================================

    Brdf brdf;
    brdf.BaseColor = baseColor.rgb;
    brdf.RoughnessAlpha = roughness * roughness;
    brdf.WorldPosition = input.WorldPosition;
    brdf.Normal = normal;
    brdf.ViewDirection = normalize(g_PerFrame.EyePosition - brdf.WorldPosition);
    brdf.NdotV = clampedDot(brdf.Normal, brdf.ViewDirection);
    brdf.DiffuseColor = lerp(brdf.BaseColor, 0.f.xxx, metalness);
    brdf.F0 = lerp(0.04.xxx, brdf.BaseColor, metalness);
    brdf.F90 = 1.f.xxx;

    brdf.Diffuse = 0.f.xxx;
    brdf.Specular = 0.f.xxx;

    // Apply static direction lights
    brdf.ApplyDirectionalLight(normalize(float3(-0.5f, -0.707f, -0.5f)), float3(1.f, 1.f, 1.f), 0.2f);
    brdf.ApplyDirectionalLight(normalize(float3(0.5f, 0.707f, 0.5f)), float3(1.f, 1.f, 1.f), 0.2f * 0.4f);

    // Apply point lights from light linked list
    uint2 lightLinkedListPosition = ScreenSpaceToLightLinkedListSpace((uint2)input.Position.xy);
    uint lightLinkIndex = g_FirstLightLink.Load(GetFirstLightLinkAddress(lightLinkedListPosition)) & NO_LIGHT_LINK;
    while (lightLinkIndex != NO_LIGHT_LINK)
    {
        LightLink lightLink = g_LightLinksHeap[lightLinkIndex];
        lightLinkIndex = lightLink.NextLightIndex();

        // Skip lights outside our depth
        if (input.Position.w < lightLink.MinDepth() || input.Position.w > lightLink.MaxDepth())
        { continue; }

        LightInfo light = g_Lights[lightLink.LightId()];
        brdf.ApplyPointLight(light);
    }

    // Compute final result
    float4 result = float4(emissive + brdf.Diffuse + brdf.Specular, baseColor.a);
    return result;
}
