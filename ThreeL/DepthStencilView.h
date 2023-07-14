#pragma once
#include <d3d12.h>

struct DepthStencilView
{
private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_DsvHandle;

public:
    DepthStencilView() = default;

    DepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle)
        : m_DsvHandle(dsvHandle)
    { }

    inline D3D12_CPU_DESCRIPTOR_HANDLE DsvHandle() const { return m_DsvHandle; }
    inline operator D3D12_CPU_DESCRIPTOR_HANDLE() const { return m_DsvHandle; }
};
