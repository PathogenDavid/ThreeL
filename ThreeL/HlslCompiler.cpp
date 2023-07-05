#include "pch.h"
#include "HlslCompiler.h"

#include <iostream>
#include <filesystem>
#include <fstream>

// Ignore SAL warnings -- IDxcResult::GetOutput is inproperly annotated
// (ppOutputName is annotated _COM_Outptr_, but should be _COM_Outptr_opt_result_maybenull_)
#pragma warning(disable : 6387)

HlslCompiler::HlslCompiler()
{
    AssertSuccess(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&m_Compiler)));
    AssertSuccess(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_CompilerUtils)));
    AssertSuccess(m_CompilerUtils->CreateDefaultIncludeHandler(&m_IncludeHandler));
}

ShaderBlobs HlslCompiler::CompileShader(std::wstring filePath, std::wstring entryPoint, std::wstring target, const std::vector<std::wstring>& defines)
{
    DxcBuffer sourceBuffer = {};
    ComPtr<IDxcBlobEncoding> source;
    AssertSuccess(m_CompilerUtils->LoadFile(filePath.c_str(), nullptr, &source));
    sourceBuffer.Ptr = source->GetBufferPointer();
    sourceBuffer.Size = source->GetBufferSize();
    sourceBuffer.Encoding = DXC_CP_ACP;

    std::vector<LPCWSTR> args =
    {
        filePath.c_str(),
        L"-E", entryPoint.c_str(),
        L"-T", target.c_str(),
        L"-Zpr", // float4x4 uses row-major storage
#ifdef DEBUG
        // Enable shader PDBs
        // -Zs makes slim PDBs, -Zi makes full ones
        // Slim PDBs are a lot smaller, but their developer experience in PIX is lacking
        // When you toggle between these you need to clear out the ShaderPdbs folder.
        //L"-Zs",
        L"-Zi",
        L"-Od",
#endif
    };

    args.reserve(args.size() + defines.size() * 2);
    for (const std::wstring& define : defines)
    {
        args.push_back(L"-D");
        args.push_back(define.c_str());
    }

    ComPtr<IDxcResult> results;
    HRESULT compileResult = m_Compiler->Compile(&sourceBuffer, args.data(), (UINT32)args.size(), m_IncludeHandler.Get(), IID_PPV_ARGS(&results));

    // Print errors if present
    ComPtr<IDxcBlobUtf8> errors;
    AssertSuccess(results->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));
    if (errors != nullptr && errors->GetStringLength() > 0)
    {
        std::wcerr << L"Diagnostics while compiling `" << entryPoint << L"` in '" << filePath << L"' for '" << target << L"':" << std::endl;
        std::cerr << std::string(errors->GetStringPointer(), errors->GetStringLength()) << std::endl;
    }

    // Fail if the compilation failed
    HRESULT overallResult;
    AssertSuccess(results->GetStatus(&overallResult));
    AssertSuccess(overallResult);
    AssertSuccess(compileResult);

    // Write out the shader PDB
#ifdef DEBUG
    {
        ComPtr<IDxcBlobUtf16> pdbPathBlob;
        ComPtr<IDxcBlob> pdb;
        AssertSuccess(results->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pdb), &pdbPathBlob));

        std::filesystem::path pdbPath(std::wstring(pdbPathBlob->GetStringPointer(), pdbPathBlob->GetStringLength()));
        Assert(!pdbPath.has_root_path());

        // Place PDBs in a folder to prevent totally polluting the working directory
        std::filesystem::path pdbFolder = std::filesystem::current_path() / "ShaderPdbs";
        std::filesystem::create_directory(pdbFolder);

        pdbPath = pdbFolder / pdbPath;
        if (!std::filesystem::exists(pdbPath))
        {
            std::ofstream f(pdbPath, std::ios::binary | std::ios::trunc);
            f.write((char*)pdb->GetBufferPointer(), pdb->GetBufferSize());
        }
    }
#endif

    // Return the compiled shader blob (and the root signature if present)
    ShaderBlobs shader;
    AssertSuccess(results->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader.ShaderBlob), nullptr));

    if (results->HasOutput(DXC_OUT_ROOT_SIGNATURE))
    { AssertSuccess(results->GetOutput(DXC_OUT_ROOT_SIGNATURE, IID_PPV_ARGS(&shader.RootSignatureBlob), nullptr)); }
    else
    { shader.RootSignatureBlob = nullptr; }

    return shader;
}
