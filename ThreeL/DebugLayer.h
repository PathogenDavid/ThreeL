#pragma once
#include <d3d12.h>

namespace DebugLayer
{
    void Setup();
    void Configure(ID3D12Device* device);

    bool IsEnabled();

    void ReportLiveObjects();
};
