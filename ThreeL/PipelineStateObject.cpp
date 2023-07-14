#include "pch.h"
#include "PipelineStateObject.h"

#include "SwapChain.h"

static const D3D12_RENDER_TARGET_BLEND_DESC BaseBlendState =
{
    .BlendEnable = false,
    .LogicOpEnable = false,
    .SrcBlend = D3D12_BLEND_ONE,
    .DestBlend = D3D12_BLEND_ZERO,
    .BlendOp = D3D12_BLEND_OP_ADD,
    .SrcBlendAlpha = D3D12_BLEND_ONE,
    .DestBlendAlpha = D3D12_BLEND_ZERO,
    .BlendOpAlpha = D3D12_BLEND_OP_ADD,
    .LogicOp = D3D12_LOGIC_OP_NOOP,
    .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL,
};

static const D3D12_DEPTH_STENCILOP_DESC BaseDepthStencilOperation =
{
    .StencilFailOp = D3D12_STENCIL_OP_KEEP,
    .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
    .StencilPassOp = D3D12_STENCIL_OP_KEEP,
    .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS,
};

const D3D12_GRAPHICS_PIPELINE_STATE_DESC PipelineStateObject::BaseDescription =
{
    .pRootSignature = nullptr,
    .VS = {},
    .PS = {},
    .DS = {},
    .HS = {},
    .GS = {},
    .StreamOutput = {},
    .BlendState =
    {
        .AlphaToCoverageEnable = false,
        .IndependentBlendEnable = false,
        .RenderTarget =
        {
            BaseBlendState,
            BaseBlendState,
            BaseBlendState,
            BaseBlendState,
            BaseBlendState,
            BaseBlendState,
            BaseBlendState,
            BaseBlendState,
        },
    },
    .SampleMask = 0xFFFFFFFF,
    .RasterizerState =
    {
        .FillMode = D3D12_FILL_MODE_SOLID,
        .CullMode = D3D12_CULL_MODE_BACK,
        // > For triangle primitives, the front face has a counter-clockwise (CCW) winding order.
        // https://github.com/KhronosGroup/glTF/tree/5de957b8b0a13c147c90d4ff569250440931872f/specification/2.0#primitiveindices
        .FrontCounterClockwise = true,
        .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
        .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
        .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
        .DepthClipEnable = true,
        .MultisampleEnable = false,
        .AntialiasedLineEnable = false,
        .ForcedSampleCount = 0,
        .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
    },
    .DepthStencilState =
    {
        .DepthEnable = true,
        .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
        .DepthFunc = D3D12_COMPARISON_FUNC_GREATER, // We use reverse-Z
        .StencilEnable = false,
        .StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
        .StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK,
        .FrontFace = BaseDepthStencilOperation,
        .BackFace = BaseDepthStencilOperation,
    },
    .InputLayout = {},
    .IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
    .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
    .NumRenderTargets = 1,
    .RTVFormats = { SwapChain::BACK_BUFFER_FORMAT },
    .DSVFormat = DEPTH_BUFFER_FORMAT,
    .SampleDesc =
    {
        .Count = 1,
        .Quality = 0,
    },
    .NodeMask = 0,
    .CachedPSO = { },
    .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
};

PipelineStateObject::PipelineStateObject(GraphicsCore& graphics, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& psoDescription, const std::wstring& name)
{
    AssertSuccess(graphics.Device()->CreateGraphicsPipelineState(&psoDescription, IID_PPV_ARGS(&m_PipelineState)));
    m_PipelineState->SetName(name.length() > 0 ? name.c_str() : L"Unnamed Graphics Pipeline State");
}

PipelineStateObject::PipelineStateObject(GraphicsCore& graphics, const D3D12_COMPUTE_PIPELINE_STATE_DESC& psoDescription, const std::wstring& name)
{
    AssertSuccess(graphics.Device()->CreateComputePipelineState(&psoDescription, IID_PPV_ARGS(&m_PipelineState)));
    m_PipelineState->SetName(name.length() > 0 ? name.c_str() : L"Unnamed Compute Pipeline State");
}
