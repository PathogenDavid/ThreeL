#pragma once
#include <d3d12.h>

struct RenderTargetView
{
private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_RtvHandle;

public:
    RenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle)
        : m_RtvHandle(rtvHandle)
    { }

    inline D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle() const { return m_RtvHandle; }
};
