#pragma once
#include "pch.h"
#include "CommandContext.h"
#include "CommandQueue.h"
#include "ResourceDescriptorManager.h"
#include "SamplerHeap.h"
#include "UploadQueue.h"

#include <dxgi1_4.h>

class GraphicsCore
{
private:
    ComPtr<ID3D12Device> m_Device;
    ComPtr<IDXGIFactory4> m_DxgiFactory;

    std::unique_ptr<ResourceDescriptorManager> m_ResourceDescriptorManager;
    std::unique_ptr<SamplerHeap> m_SamplerHeap;

    std::unique_ptr<GraphicsQueue> m_GraphicsQueue;
    std::unique_ptr<ComputeQueue> m_ComputeQueue;
    std::unique_ptr<UploadQueue> m_UploadQueue;
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

    inline ResourceDescriptorManager& GetResourceDescriptorManager() const
    {
        return *m_ResourceDescriptorManager;
    }

    inline SamplerHeap& GetSamplerHeap() const
    {
        return *m_SamplerHeap;
    }

    inline GraphicsQueue& GetGraphicsQueue() const
    {
        return *m_GraphicsQueue;
    }

    inline ComputeQueue& GetComputeQueue() const
    {
        return *m_ComputeQueue;
    }

    inline UploadQueue& GetUploadQueue() const
    {
        return *m_UploadQueue;
    }

    //! Waits for all outstanding GPU work to complete, do not use for anything that happens frequently
    inline void WaitForGpuIdle() const
    {
        m_GraphicsQueue->QueueSyncPoint().Wait();
        m_ComputeQueue->QueueSyncPoint().Wait();
        m_UploadQueue->QueueSyncPoint().Wait();
    }
};
