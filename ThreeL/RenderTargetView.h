#pragma once
#include <d3d12.h>

struct RenderTargetView
{
    friend class BackBuffer;

private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_RtvHandle;

    RenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle)
    {
        m_RtvHandle = rtvHandle;
    }

public:
    inline D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle()
    {
        return m_RtvHandle;
    }
};
