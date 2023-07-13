#pragma once
#include <d3d12.h>
#include <string>

class GraphicsCore;

#define DEPTH_BUFFER_FORMAT DXGI_FORMAT_D32_FLOAT

class PipelineStateObject
{
private:
    ComPtr<ID3D12PipelineState> m_PipelineState;

public:
    static const D3D12_GRAPHICS_PIPELINE_STATE_DESC BaseDescription;

    PipelineStateObject() = default;
    PipelineStateObject(GraphicsCore& graphics, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& psoDescription, const std::wstring& name);
    PipelineStateObject(GraphicsCore& graphics, const D3D12_COMPUTE_PIPELINE_STATE_DESC& psoDescription, const std::wstring& name);

    inline ID3D12PipelineState* Get() const { return m_PipelineState.Get(); }
    inline operator ID3D12PipelineState* () const { return m_PipelineState.Get(); }
};
