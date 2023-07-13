#include "pch.h"
#include "Utilities.h"

#include <filesystem>

void SetWorkingDirectoryToAppDirectory()
{
    WCHAR* path = nullptr;
    DWORD length = 512;
    DWORD result;
    do
    {
        length *= 2;
        delete[] path;
        path = new WCHAR[length];
        result = GetModuleFileNameW(nullptr, path, length);
        AssertWinError(result > 0);
    } while (result == length);

    std::filesystem::path p(path);
    std::filesystem::current_path(p.parent_path());
}

std::wstring GetD3DObjectName(ID3D12Object* object)
{
    UINT size = 0;
    HRESULT result = object->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, nullptr);

    if (SUCCEEDED(result) && size > 0)
    {
        std::wstring name(size, L'\0');
        result = object->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name.data());
        AssertSuccess(result);
        return name;
    }

    return std::format(L"<Unnamed 0x{}>", (void*)object);
}
