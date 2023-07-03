#include "pch.h"
#include "ResourceManager.h"

#include "HlslCompiler.h"

ResourceManager::ResourceManager(GraphicsCore& graphics)
    : Graphics(graphics), PbrMaterials(graphics), MeshHeap(graphics)
{
    HlslCompiler hlslCompiler;

    // Compile all shaders
    ShaderBlobs pbrVs = hlslCompiler.CompileShader(L"Shaders/Pbr.hlsl", L"VsMain", L"vs_6_0");
    ShaderBlobs pbrPs = hlslCompiler.CompileShader(L"Shaders/Pbr.hlsl", L"PsMain", L"ps_6_0");

    // Create root signatures
    PbrRootSignature = RootSignature(Graphics, pbrVs, L"PBR Root Signature");

    // Create PBR pipeline state objects
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pbrDescription = PipelineStateObject::BaseDescription;
    pbrDescription.pRootSignature = PbrRootSignature.Get();
    pbrDescription.VS = pbrVs.ShaderBytecode();
    pbrDescription.PS = pbrPs.ShaderBytecode();

    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        // SemanticName, SemanticIndex, Format, InputSlot
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, MeshInputSlot::Position },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, MeshInputSlot::Normal },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, MeshInputSlot::Uv0 },
    };
    pbrDescription.InputLayout = { inputLayout, (UINT)std::size(inputLayout) };

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
    PbrBlendOnDoubleSided = PipelineStateObject(Graphics, pbrDescription, L"PBR PSO - Blended Single Sided");
}