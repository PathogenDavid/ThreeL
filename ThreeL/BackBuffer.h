#pragma once
#include "GpuResource.h"
#include "RenderTargetView.h"

#include <d3d12.h>

class BackBuffer final : public GpuResource
{
    friend class SwapChain;

private:
    RenderTargetView m_Rtv;

    BackBuffer()
        : m_Rtv({})
    {
    }

    BackBuffer(ComPtr<ID3D12Resource> renderTarget, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle)
        : m_Rtv(rtvHandle)
    {
        m_Resource = renderTarget;
        m_CurrentState = D3D12_RESOURCE_STATE_PRESENT;
    }

    inline void AssertIsInPresentState() const
    {
        Assert(m_CurrentState == D3D12_RESOURCE_STATE_PRESENT);
    }

public:
    inline RenderTargetView GetRtv() const
    {
        return m_Rtv;
    }
};
