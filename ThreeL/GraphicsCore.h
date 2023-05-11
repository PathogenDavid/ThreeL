#pragma once
#include "CommandContext.h"
#include "CommandQueue.h"
#include "ResourceDescriptorManager.h"
#include "Util.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <memory>

class GraphicsCore
{
private:
    ComPtr<ID3D12Device> m_Device;
    ComPtr<IDXGIFactory4> m_DxgiFactory;

    std::unique_ptr<ResourceDescriptorManager> m_ResourceDescriptorManager;

    std::unique_ptr<GraphicsQueue> m_GraphicsQueue;
    std::unique_ptr<ComputeQueue> m_ComputeQueue;
public:
    GraphicsCore();

    inline const ComPtr<ID3D12Device>& GetDevice() const
    {
        return m_Device;
    }

    inline const ComPtr<IDXGIFactory4>& GetDxgiFactory() const
    {
        return m_DxgiFactory;
    }

    inline ResourceDescriptorManager& GetResourceDescriptorManager()
    {
        return *m_ResourceDescriptorManager;
    }

    inline GraphicsQueue& GetGraphicsQueue()
    {
        return *m_GraphicsQueue;
    }

    inline ComputeQueue& GetComputeQueue()
    {
        return *m_ComputeQueue;
    }
};
