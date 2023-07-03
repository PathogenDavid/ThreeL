#pragma once
#include "pch.h"

#include "DepthStencilView.h"
#include "DynamicResourceDescriptor.h"
#include "GpuResource.h"
#include "Math.h"
#include "ResourceDescriptor.h"

class GraphicsCore;

class DepthStencilBuffer : public GpuResource
{
private:
    GraphicsCore& m_Graphics;
    ComPtr<ID3D12DescriptorHeap> m_DescriptorHeap;
    std::wstring m_Name;

    DXGI_FORMAT m_Format;
    DXGI_FORMAT m_DepthSrvFormat;
    DXGI_FORMAT m_StencilSrvFormat;
    uint2 m_Size;
    float m_DepthClearValue;
    uint8_t m_StencilClearValue;

    DepthStencilView m_View;
    DepthStencilView m_DepthReadOnlyView;
    DepthStencilView m_StencilReadOnlyView;
    DepthStencilView m_ReadOnlyView;
    DynamicResourceDescriptor m_DepthShaderResourceView;
    DynamicResourceDescriptor m_StencilShaderResourceView;

public:
    DepthStencilBuffer(GraphicsCore& graphics, const std::wstring& name, uint2 size, DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT, float depthClearValue = 0.f, uint8_t stencilClearValue = 0);

    //! Resizes the depth stencil buffer
    //! You must ensure all outstanding uses of this buffer on the GPU are complete before calling this method
    void Resize(uint2 newSize);

    inline float DepthClearValue() const { return m_DepthClearValue; }
    inline uint8_t StencilClearValue() const { return m_StencilClearValue; }
    inline DepthStencilView View() const { return m_View; }
    inline DepthStencilView DepthReadOnlyView() const { return m_DepthReadOnlyView; }
    inline DepthStencilView StencilReadOnlyView() const { return m_StencilReadOnlyView; }
    inline DepthStencilView ReadOnlyView() const { return m_ReadOnlyView; }
    inline ResourceDescriptor DepthShaderResourceView() const { return m_DepthShaderResourceView.ResourceDescriptor(); }

    //! Returned handle is undefined if the format does not specify a stencil buffer
    inline ResourceDescriptor StencilShaderResourceView() const
    {
        Assert(m_StencilSrvFormat != DXGI_FORMAT_UNKNOWN);
        return m_StencilShaderResourceView.ResourceDescriptor();
    }
};
