#pragma once
#include "pch.h"

struct ShaderBlobs
{
    ComPtr<ID3DBlob> ShaderBlob;
    ComPtr<ID3DBlob> RootSignatureBlob;

    inline D3D12_SHADER_BYTECODE ShaderBytecode()
    {
        return
        {
            .pShaderBytecode = ShaderBlob->GetBufferPointer(),
            .BytecodeLength = ShaderBlob->GetBufferSize()
        };
    }
};

class HlslCompiler
{
private:
    ComPtr<IDxcCompiler3> m_Compiler;
    ComPtr<IDxcUtils> m_CompilerUtils;
    ComPtr<IDxcIncludeHandler> m_IncludeHandler;

public:
    HlslCompiler();
    ShaderBlobs CompileShader(std::wstring filePath, std::wstring entryPoint, std::wstring target, const std::vector<std::wstring>& defines = {});
};
