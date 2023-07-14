#include "pch.h"
#include "RootSignature.h"

#include "GraphicsCore.h"
#include "HlslCompiler.h"

RootSignature::RootSignature(GraphicsCore& graphics, ID3DBlob* rootSignatureBlob, const std::wstring& name)
{
    AssertSuccess(graphics.Device()->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));
    m_RootSignature->SetName(name.c_str());
}

RootSignature::RootSignature(GraphicsCore& graphics, const ShaderBlobs& shader, const std::wstring& name)
    : RootSignature(graphics, shader.RootSignatureBlob.Get(), name)
{
}
