#pragma once
#include "pch.h"

class GraphicsCore;
struct ShaderBlobs;

class RootSignature
{
private:
    ComPtr<ID3D12RootSignature> m_RootSignature;

public:
    RootSignature() = default;
    RootSignature(GraphicsCore& graphics, ID3DBlob* rootSignatureBlob, const std::wstring& name);
    RootSignature(GraphicsCore& graphics, const ShaderBlobs& shader, const std::wstring& name);

    inline ID3D12RootSignature* Get() const { return m_RootSignature.Get(); }
    inline operator ID3D12RootSignature* () const { return m_RootSignature.Get(); }
};
