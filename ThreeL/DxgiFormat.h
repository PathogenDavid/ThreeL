#pragma once
#include <dxgiformat.h>

namespace DxgiFormat
{
    bool IsUnorm(DXGI_FORMAT format);
    bool IsSrgb(DXGI_FORMAT format);
    const char* Name(DXGI_FORMAT format);
    const wchar_t* NameW(DXGI_FORMAT format);
}
