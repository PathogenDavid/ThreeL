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

std::wstring DescribeResourceState(D3D12_RESOURCE_STATES states)
{
    if (states == D3D12_RESOURCE_STATE_COMMON)
    {
        static_assert(D3D12_RESOURCE_STATE_COMMON == D3D12_RESOURCE_STATE_PRESENT);
        return L"COMMON/PRESENT";
    }
    else if (states == D3D12_RESOURCE_STATE_GENERIC_READ)
    { return L"GENERIC_READ"; }
    else if (states == D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE)
    { return L"ALL_SHADER_RESOURCE"; }

    std::wstring result;
#define CHECK(state) if ((states & D3D12_RESOURCE_STATE_ ## state) != 0) \
    { \
        if (result.size() > 0) { result += L" | "; } \
        result += L#state; \
    }
    CHECK(VERTEX_AND_CONSTANT_BUFFER);
    CHECK(INDEX_BUFFER);
    CHECK(RENDER_TARGET);
    CHECK(UNORDERED_ACCESS);
    CHECK(DEPTH_WRITE);
    CHECK(DEPTH_READ);
    CHECK(NON_PIXEL_SHADER_RESOURCE);
    CHECK(PIXEL_SHADER_RESOURCE);
    CHECK(STREAM_OUT);
    CHECK(INDIRECT_ARGUMENT);
    CHECK(COPY_DEST);
    CHECK(COPY_SOURCE);
    CHECK(RESOLVE_DEST);
    CHECK(RESOLVE_SOURCE);
    CHECK(RAYTRACING_ACCELERATION_STRUCTURE);
    CHECK(SHADING_RATE_SOURCE);
    CHECK(PREDICATION);
    CHECK(VIDEO_DECODE_READ);
    CHECK(VIDEO_DECODE_WRITE);
    CHECK(VIDEO_PROCESS_READ);
    CHECK(VIDEO_PROCESS_WRITE);
    CHECK(VIDEO_ENCODE_READ);
    CHECK(VIDEO_ENCODE_WRITE);
    return result;
}
