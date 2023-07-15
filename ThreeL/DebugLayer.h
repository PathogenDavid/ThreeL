#pragma once
#include <d3d12.h>

namespace DebugLayer
{
    void Setup();
    void Configure(ID3D12Device* device);

    bool IsEnabled();
    //! Note: Return value is what we tried to configure, could be overriden with d3dconfig.
    bool IsGpuBasedValidationEnabled();

    void ReportLiveObjects();

    std::wstring GetExtraWindowTitleInfo();
};
