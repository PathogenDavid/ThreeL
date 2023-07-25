#pragma once
#include "pch.h"
#include "CommandContext.h"
#include "CommandQueue.h"
#include "GraphicsCore.h"
#include "ResourceDescriptorManager.h"
#include "SamplerHeap.h"
#include "UploadQueue.h"

#include <dxgi1_4.h>

class GraphicsCore
{
private:
    ComPtr<ID3D12Device> m_Device;
    ComPtr<IDXGIFactory4> m_DxgiFactory;
    DXGI_ADAPTER_DESC1 m_AdapterDescription;

    std::unique_ptr<ResourceDescriptorManager> m_ResourceDescriptorManager;
    std::unique_ptr<SamplerHeap> m_SamplerHeap;

    std::unique_ptr<GraphicsQueue> m_GraphicsQueue;
    std::unique_ptr<ComputeQueue> m_ComputeQueue;
    std::unique_ptr<UploadQueue> m_UploadQueue;

    ComPtr<ID3D12CommandSignature> m_DrawIndirectCommandSignature;
    ComPtr<ID3D12CommandSignature> m_DrawIndexedIndirectCommandSignature;
    ComPtr<ID3D12CommandSignature> m_DispatchIndirectCommandSignature;
public:
    GraphicsCore();
    GraphicsCore(const GraphicsCore&) = delete;

    inline ID3D12Device* Device() const { return m_Device.Get(); }
    inline IDXGIFactory4* DxgiFactory() const { return m_DxgiFactory.Get(); }
    inline ResourceDescriptorManager& ResourceDescriptorManager() const { return *m_ResourceDescriptorManager; }
    inline SamplerHeap& SamplerHeap() const { return *m_SamplerHeap; }
    inline GraphicsQueue& GraphicsQueue() const { return *m_GraphicsQueue; }
    inline ComputeQueue& ComputeQueue() const { return *m_ComputeQueue; }
    inline UploadQueue& UploadQueue() const { return *m_UploadQueue; }

    inline ID3D12CommandSignature* DrawIndirectCommandSignature() const { return m_DrawIndirectCommandSignature.Get(); }
    inline ID3D12CommandSignature* DrawIndexedIndirectCommandSignature() const { return m_DrawIndexedIndirectCommandSignature.Get(); }
    inline ID3D12CommandSignature* DispatchIndirectCommandSignature() const { return m_DispatchIndirectCommandSignature.Get(); }

    inline bool IsIntel() const { return m_AdapterDescription.VendorId == 0x8086; }
    inline const wchar_t* AdapterName() const { return m_AdapterDescription.Description; }

    //! Waits for all outstanding GPU work to complete, do not use for anything that happens frequently
    inline void WaitForGpuIdle() const
    {
        m_GraphicsQueue->QueueSyncPoint().Wait();
        m_ComputeQueue->QueueSyncPoint().Wait();
        m_UploadQueue->QueueSyncPoint().Wait();
    }
};
