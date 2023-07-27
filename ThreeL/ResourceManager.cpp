#include "pch.h"
#include "ResourceManager.h"

#include "GraphicsCore.h"
#include "HlslCompiler.h"

ResourceManager::ResourceManager(GraphicsCore& graphics)
    : Graphics(graphics), PbrMaterials(graphics), MeshHeap(graphics)
{
    HlslCompiler hlslCompiler;

    BitonicSort = ::BitonicSort(Graphics, hlslCompiler);

    // Compile all shaders
    ShaderBlobs pbrVs = hlslCompiler.CompileShader(L"Shaders/Pbr.hlsl", L"VsMain", L"vs_6_0");
    ShaderBlobs pbrPs = hlslCompiler.CompileShader(L"Shaders/Pbr.hlsl", L"PsMain", L"ps_6_0");
    ShaderBlobs pbrPsLightDebug = hlslCompiler.CompileShader(L"Shaders/Pbr.hlsl", L"PsMain", L"ps_6_0", { L"DEBUG_LIGHT_BOUNDARIES" });

    ShaderBlobs depthOnlyVs = hlslCompiler.CompileShader(L"Shaders/DepthOnly.hlsl", L"VsMain", L"vs_6_0");
    ShaderBlobs depthOnlyPs = hlslCompiler.CompileShader(L"Shaders/DepthOnly.hlsl", L"PsMain", L"ps_6_0");

    ShaderBlobs fullScreenQuadVs = hlslCompiler.CompileShader(L"Shaders/FullScreenQuad.vs.hlsl", L"VsMain", L"vs_6_0");

    ShaderBlobs depthDownsamplePs = hlslCompiler.CompileShader(L"Shaders/DepthDownsample.ps.hlsl", L"PsMain", L"ps_6_0");

    ShaderBlobs generateMipMapsCsUnorm = hlslCompiler.CompileShader(L"Shaders/GenerateMipmapChain.cs.hlsl", L"Main", L"cs_6_0", { L"GENERATE_UNORM_MIPMAP_CHAIN" });
    ShaderBlobs generateMipMapsCsFloat = hlslCompiler.CompileShader(L"Shaders/GenerateMipmapChain.cs.hlsl", L"Main", L"cs_6_0");

    ShaderBlobs lightLinkedListFillVs = hlslCompiler.CompileShader(L"Shaders/LightLinkedListFill.hlsl", L"VsMain", L"vs_6_0");
    ShaderBlobs lightLinkedListFillPs = hlslCompiler.CompileShader(L"Shaders/LightLinkedListFill.hlsl", L"PsMain", L"ps_6_0");

    ShaderBlobs lightLinkedListDebugPs = hlslCompiler.CompileShader(L"Shaders/LightLinkedListDebug.ps.hlsl", L"PsMain", L"ps_6_0");

    std::vector<std::wstring> lightLinkedListStatsCsDefines;
    // My Intel UHD 620 is choking on the groupshared optimization in this shader.
    // I've also seen a AMD RX 6700 XT choking on this in a different way (light count is randomly corrupted), so let's just disable the optimization for now.
    //if (Graphics.IsIntel())
    lightLinkedListStatsCsDefines.push_back(L"NO_GROUPSHARED");
    ShaderBlobs lightLinkedListStatsCs = hlslCompiler.CompileShader(L"Shaders/LightLinkedListStats.cs.hlsl", L"Main", L"cs_6_0", lightLinkedListStatsCsDefines);

    ShaderBlobs lightSpritesVs = hlslCompiler.CompileShader(L"Shaders/LightSprites.hlsl", L"VsMain", L"vs_6_0");
    ShaderBlobs lightSpritesPs = hlslCompiler.CompileShader(L"Shaders/LightSprites.hlsl", L"PsMain", L"ps_6_0");

    ShaderBlobs particleSystemSpawn = hlslCompiler.CompileShader(L"Shaders/ParticleSystem.cs.hlsl", L"MainSpawn", L"cs_6_0");
    ShaderBlobs particleSystemUpdate = hlslCompiler.CompileShader(L"Shaders/ParticleSystem.cs.hlsl", L"MainUpdate", L"cs_6_0");
    ShaderBlobs particleSystemPrepareDrawIndirect = hlslCompiler.CompileShader(L"Shaders/ParticleSystem.cs.hlsl", L"MainPrepareDrawIndirect", L"cs_6_0");

    ShaderBlobs particleRenderVs = hlslCompiler.CompileShader(L"Shaders/ParticleRender.hlsl", L"VsMainParticle", L"vs_6_0");
    ShaderBlobs particleRenderPs = hlslCompiler.CompileShader(L"Shaders/ParticleRender.hlsl", L"PsMain", L"ps_6_0");

    // Create root signatures
    PbrRootSignature = RootSignature(Graphics, pbrVs, L"PBR Root Signature");
    DepthOnlyRootSignature = RootSignature(Graphics, depthOnlyVs, L"DepthOnly Root Signature");
    DepthDownsampleRootSignature = RootSignature(Graphics, depthDownsamplePs, L"DepthDownsample Root Signature");
    GenerateMipMapsRootSignature = RootSignature(Graphics, generateMipMapsCsUnorm, L"GenerateMipMaps Root Signature");
    LightLinkedListFillRootSignature = RootSignature(Graphics, lightLinkedListFillVs, L"LightLinkedList Fill Root Signature");
    LightLinkedListDebugRootSignature = RootSignature(Graphics, lightLinkedListDebugPs, L"LightLinkedList Debug Root Signature");
    LightLinkedListStatsRootSignature = RootSignature(Graphics, lightLinkedListStatsCs, L"LightLinkedList Statistics Root Signature");
    LightSpritesRootSignature = RootSignature(Graphics, lightSpritesVs, L"Light Sprites Root Signature");
    ParticleSystemRootSignature = RootSignature(Graphics, particleSystemSpawn, L"Particle System Root Signature");
    ParticleRenderRootSignature = RootSignature(Graphics, particleRenderVs, L"Particle Render Root Signature");

    // Create PBR pipeline state objects
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pbrDescription = PipelineStateObject::BaseDescription;
        pbrDescription.pRootSignature = PbrRootSignature.Get();
        pbrDescription.VS = pbrVs.ShaderBytecode();
        pbrDescription.PS = pbrPs.ShaderBytecode();

        // Depth pre-pass
        pbrDescription.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_EQUAL;
        pbrDescription.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

        D3D12_INPUT_ELEMENT_DESC inputLayout[] =
        {
            // SemanticName, SemanticIndex, Format, InputSlot
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, MeshInputSlot::Position },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, MeshInputSlot::Normal },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, MeshInputSlot::Uv0 },
        };
        pbrDescription.InputLayout = { inputLayout, (UINT)std::size(inputLayout) };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pbrLightDebugDescription = pbrDescription;

        // Create opaque PSOs
        PbrBlendOffSingleSided = PipelineStateObject(Graphics, pbrDescription, L"PBR PSO - Opaque Single Sided");
        pbrDescription.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        PbrBlendOffDoubleSided = PipelineStateObject(Graphics, pbrDescription, L"PBR PSO - Opaque Double Sided");

        // Create alpha-blended PSOs
        pbrDescription.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        pbrDescription.BlendState.RenderTarget[0] =
        {
            .BlendEnable = true,
            .LogicOpEnable = false,
            .SrcBlend = D3D12_BLEND_SRC_ALPHA,
            .DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
            .BlendOp = D3D12_BLEND_OP_ADD,
            .SrcBlendAlpha = D3D12_BLEND_ONE,
            .DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA,
            .BlendOpAlpha = D3D12_BLEND_OP_ADD,
            .LogicOp = D3D12_LOGIC_OP_NOOP,
            .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
        };
        PbrBlendOnDoubleSided = PipelineStateObject(Graphics, pbrDescription, L"PBR PSO - Blended Double Sided");
        pbrDescription.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        PbrBlendOnSingleSided = PipelineStateObject(Graphics, pbrDescription, L"PBR PSO - Blended Single Sided");

        // Create light boundary debug PSOs
        pbrLightDebugDescription.PS = pbrPsLightDebug.ShaderBytecode();
        PbrLightDebugSingleSided = PipelineStateObject(Graphics, pbrLightDebugDescription, L"PBR PSO - Light Debug Single Sided");
        pbrLightDebugDescription.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        PbrLightDebugDoubleSided = PipelineStateObject(Graphics, pbrLightDebugDescription, L"PBR PSO - Light Debug Double Sided");
    }

    // Create DepthOnly pipeline state object
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC depthOnlyDescription = PipelineStateObject::BaseDescription;
        depthOnlyDescription.pRootSignature = DepthOnlyRootSignature.Get();
        depthOnlyDescription.VS = depthOnlyVs.ShaderBytecode();
        depthOnlyDescription.PS = depthOnlyPs.ShaderBytecode();
        depthOnlyDescription.NumRenderTargets = 0;
        depthOnlyDescription.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;

        D3D12_INPUT_ELEMENT_DESC inputLayout[] =
        {
            // SemanticName, SemanticIndex, Format, InputSlot
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, MeshInputSlot::Position },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, MeshInputSlot::Uv0 },
        };
        depthOnlyDescription.InputLayout = { inputLayout, (UINT)std::size(inputLayout) };

        DepthOnlySingleSided = PipelineStateObject(Graphics, depthOnlyDescription, L"DepthOnly PSO - Single Sided");
        depthOnlyDescription.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        DepthOnlyDoubleSided = PipelineStateObject(Graphics, depthOnlyDescription, L"DepthOnly PSO - Double Sided");
    }

    // Create DepthDownsample pipeline state object
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC depthDownsampleDescription = PipelineStateObject::BaseDescription;
        depthDownsampleDescription.pRootSignature = DepthDownsampleRootSignature.Get();
        depthDownsampleDescription.VS = fullScreenQuadVs.ShaderBytecode();
        depthDownsampleDescription.PS = depthDownsamplePs.ShaderBytecode();
        depthDownsampleDescription.NumRenderTargets = 0;
        depthDownsampleDescription.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
        depthDownsampleDescription.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

        DepthDownsample = PipelineStateObject(Graphics, depthDownsampleDescription, L"DepthDownsample PSO");
    }

    // Create GenerateMipMaps pipeline state object
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC generateMipMapsDescription =
        {
            .pRootSignature = GenerateMipMapsRootSignature.Get(),
            .CS = generateMipMapsCsUnorm.ShaderBytecode(),
        };
        GenerateMipMapsUnorm = PipelineStateObject(Graphics, generateMipMapsDescription, L"GenerateMipMaps PSO - UNorm");
        generateMipMapsDescription.CS = generateMipMapsCsFloat.ShaderBytecode();
        GenerateMipMapsFloat = PipelineStateObject(Graphics, generateMipMapsDescription, L"GenerateMipMaps PSO - Float");
    }

    // Create LightLinkedListFill pipeline state object
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC description = PipelineStateObject::BaseDescription;
        description.pRootSignature = LightLinkedListFillRootSignature.Get();
        description.VS = lightLinkedListFillVs.ShaderBytecode();
        description.PS = lightLinkedListFillPs.ShaderBytecode();
        description.NumRenderTargets = 0;
        description.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
        description.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
        description.DepthStencilState.DepthEnable = false;
        description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

        D3D12_INPUT_ELEMENT_DESC inputLayout[] =
        {
            // SemanticName, SemanticIndex, Format, InputSlot
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, MeshInputSlot::Position },
        };
        description.InputLayout = { inputLayout, (UINT)std::size(inputLayout) };

        LightLinkedListFill = PipelineStateObject(Graphics, description, L"LightLinkedList Fill PSO");
    }

    // Create LightLinkedListDebug pipeline state object
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC description = PipelineStateObject::BaseDescription;
        description.pRootSignature = LightLinkedListDebugRootSignature.Get();
        description.VS = fullScreenQuadVs.ShaderBytecode();
        description.PS = lightLinkedListDebugPs.ShaderBytecode();
        description.DepthStencilState.DepthEnable = false;
        description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        description.BlendState.RenderTarget[0] =
        {
            .BlendEnable = true,
            .LogicOpEnable = false,
            .SrcBlend = D3D12_BLEND_SRC_ALPHA,
            .DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
            .BlendOp = D3D12_BLEND_OP_ADD,
            .SrcBlendAlpha = D3D12_BLEND_ONE,
            .DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA,
            .BlendOpAlpha = D3D12_BLEND_OP_ADD,
            .LogicOp = D3D12_LOGIC_OP_NOOP,
            .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
        };

        LightLinkedListDebug = PipelineStateObject(Graphics, description, L"LightLinkedList Debug PSO");
    }

    // Create LightLinkedListStats pipeline state object
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC generateMipMapsDescription =
        {
            .pRootSignature = LightLinkedListStatsRootSignature.Get(),
            .CS = lightLinkedListStatsCs.ShaderBytecode(),
        };
        LightLinkedListStats = PipelineStateObject(Graphics, generateMipMapsDescription, L"LightLinkedList Statistics PSO");
    }

    // Create LightSprites pipeline state object
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC description = PipelineStateObject::BaseDescription;
        description.pRootSignature = LightSpritesRootSignature.Get();
        description.VS = lightSpritesVs.ShaderBytecode();
        description.PS = lightSpritesPs.ShaderBytecode();
        LightSprites = PipelineStateObject(Graphics, description, L"Light Sprites PSO");
    }

    // Create ParticleSystem pipeline state objects
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC description =
        {
            .pRootSignature = ParticleSystemRootSignature.Get(),
            .CS = particleSystemSpawn.ShaderBytecode(),
        };
        ParticleSystemSpawn = PipelineStateObject(Graphics, description, L"Particle System Spawn PSO");

        description.CS = particleSystemUpdate.ShaderBytecode();
        ParticleSystemUpdate = PipelineStateObject(Graphics, description, L"Particle System Update PSO");

        description.CS = particleSystemPrepareDrawIndirect.ShaderBytecode();
        ParticleSystemPrepareDrawIndirect = PipelineStateObject(Graphics, description, L"Particle System Prepare DrawIndirect PSO");
    }

    // Create ParticleRender pipeline state object
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC description = PipelineStateObject::BaseDescription;
        description.pRootSignature = ParticleRenderRootSignature.Get();
        description.VS = particleRenderVs.ShaderBytecode();
        description.PS = particleRenderPs.ShaderBytecode();
        description.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        description.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        description.BlendState.RenderTarget[0] =
        {
            .BlendEnable = true,
            .LogicOpEnable = false,
            .SrcBlend = D3D12_BLEND_SRC_ALPHA, // It's tempting to use pre-multiplied alpha, but the textures only *sort-of* look like they're pre-multiplied.
            .DestBlend = D3D12_BLEND_INV_SRC_ALPHA,
            .BlendOp = D3D12_BLEND_OP_ADD,
            .SrcBlendAlpha = D3D12_BLEND_ONE,
            .DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA,
            .BlendOpAlpha = D3D12_BLEND_OP_ADD,
            .LogicOp = D3D12_LOGIC_OP_NOOP,
            .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
        };

        ParticleRender = PipelineStateObject(Graphics, description, L"Particle Render PSO");
    }
}
