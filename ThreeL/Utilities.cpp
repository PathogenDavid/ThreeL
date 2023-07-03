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
