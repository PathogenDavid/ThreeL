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

double GetHumanFriendlySize(size_t sizeBytes, const char*& sizeUnits)
{
    double size = (double)sizeBytes;
    sizeUnits = "bytes";
    if (size < 1024.0)
        return size;

    size /= 1024.0;
    sizeUnits = "KB";
    if (size < 1024.0)
        return size;

    size /= 1024.0;
    sizeUnits = "MB";
    if (size < 1024.0)
        return size;

    size /= 1024.0;
    sizeUnits = "GB";
    return size;
}

std::wstring GetD3DObjectName(ID3D12Object* object)
{
    if (object == nullptr)
    { return L"<null>"; }

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

D3D12_RESOURCE_DESC DescribeBufferResource(uint64_t sizeBytes, D3D12_RESOURCE_FLAGS flags, uint64_t alignment)
{
    return
    {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = alignment,
        .Width = sizeBytes,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = { .Count = 1, .Quality = 0 },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = flags,
    };
}
