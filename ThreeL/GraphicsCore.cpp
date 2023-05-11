#include "GraphicsCore.h"

#include "Assert.h"
#include "CommandQueue.h"
#include "DebugLayer.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <pix3.h>

GraphicsCore::GraphicsCore()
{
    //---------------------------------------------------------------------------------------------------------
    // Enable PIX capture runtime
    //---------------------------------------------------------------------------------------------------------
#if defined(USE_PIX) && true
    PIXLoadLatestWinPixGpuCapturerLibrary();
#endif

    //---------------------------------------------------------------------------------------------------------
    // Enable debug layer
    //---------------------------------------------------------------------------------------------------------
#ifdef DEBUG
    DebugLayer::Setup();
#endif

    //---------------------------------------------------------------------------------------------------------
    // Enumerate DXGI Adapter
    //---------------------------------------------------------------------------------------------------------
    const D3D_FEATURE_LEVEL requiredFeatureLevel = D3D_FEATURE_LEVEL_11_0;
    ComPtr<IDXGIAdapter1> adapter;
    {
        AssertSuccess(CreateDXGIFactory2(DebugLayer::IsEnabled() ? DXGI_CREATE_FACTORY_DEBUG : 0, IID_PPV_ARGS(&m_DxgiFactory)));

        ComPtr<IDXGIFactory6> dxgiFactory6;
        bool have6 = SUCCEEDED(m_DxgiFactory->QueryInterface(IID_PPV_ARGS(&dxgiFactory6)));

        for (uint32_t i = 0; ; i++)
        {
            // Enumerate the next adapter
            HRESULT hr;
            if (have6)
                hr = dxgiFactory6->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf()));
            else
                hr = m_DxgiFactory->EnumAdapters1(i, adapter.ReleaseAndGetAddressOf());

            if (hr == DXGI_ERROR_NOT_FOUND)
                break;

            AssertSuccess(hr);

            DXGI_ADAPTER_DESC1 description;
            if (FAILED(adapter->GetDesc1(&description)))
            {
                Assert(false);
                continue;
            }

            // Don't accept the Microsoft Basic Render Driver
            // https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/d3d10-graphics-programming-guide-dxgi#new-info-about-enumerating-adapters-for-windows-8
            if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
            {
                wprintf(L"Rejecting adapter '%s': Adapter is fallback software renderer.", description.Description);
                continue;
            }

            // Ensure device supports Direct3D 12 with the required feature level
            if (FAILED(D3D12CreateDevice(adapter.Get(), requiredFeatureLevel, __uuidof(ID3D12Device), nullptr)))
            {
                wprintf(L"Rejecting adapter '%s': Adapter does not support Direct3D 12 at feature level %x.", description.Description, requiredFeatureLevel);
                continue;
            }

            // If we got this far the adapter is acceptable
            break;
        }

        if (adapter == nullptr)
        {
            MessageBoxW(nullptr, L"Failed to find suitable GPU.", L"Fatal Error", MB_ICONERROR);
            exit(1);
        }
    }

    //---------------------------------------------------------------------------------------------------------
    // Create the Direct3D device
    //---------------------------------------------------------------------------------------------------------
    AssertSuccess(D3D12CreateDevice(adapter.Get(), requiredFeatureLevel, IID_PPV_ARGS(&m_Device)));
    m_Device->SetName(L"ARES GraphicsCore Device");
    DebugLayer::Configure(m_Device.Get());

    //---------------------------------------------------------------------------------------------------------
    // Allocate the GPU CBV/SRV/UAV descriptor heap
    //---------------------------------------------------------------------------------------------------------
    // We only allocate a single GPU descriptor heap because switching between them is expensive on many platforms.
    // Intel:
    //    Minimize descriptor heap changes. Changing descriptor heaps severely stalls the graphics pipeline. Ideally, all resources will have views appropriated out of one descriptor heap.
    //    https://software.intel.com/content/www/us/en/develop/articles/developer-and-optimization-guide-for-intel-processor-graphics-gen11-api.html#inpage-nav-3-1
    // Nvidia:
    //    Make sure to use just one CBV/SRV/UAV/descriptor heap as a ring-buffer for all frames if you want to aim at running parallel asynchronous compute and graphics workloads
    //    https://developer.nvidia.com/dx12-dos-and-donts#worksubmit
    // Microsoft:
    //    On some hardware, this can be an expensive operation, requiring a GPU stall to flush all work that depends on the currently bound descriptor heap.
    //    As a result, if descriptor heaps must be changed, applications should try to do so when the GPU workload is relatively light, perhaps limiting changes to the start of a command list.
    //    https://docs.microsoft.com/en-us/windows/win32/direct3d12/descriptor-heaps-overview#switching-heaps
    // AMD doesn't seem to care since they don't mention it in any best practice guides.
    m_ResourceDescriptorManager = std::make_unique<ResourceDescriptorManager>(m_Device);

    //---------------------------------------------------------------------------------------------------------
    // Initialize queues
    //---------------------------------------------------------------------------------------------------------
    m_GraphicsQueue = std::make_unique<GraphicsQueue>(*this);
    m_ComputeQueue = std::make_unique<ComputeQueue>(*this);
}

//---------------------------------------------------------------------------------------------------------------------------------------------------------
// GPU device hints
//---------------------------------------------------------------------------------------------------------------------------------------------------------
// These exported symbols hint to GPU drivers that we're a high-performance app
// They're really only relevant on Windows 7 and Windows 10 older than 1803, which do not allow expressing GPU preference during adapter enumeration.
// (On modern versions of Windows we use IDXGIFactory6::EnumAdapterByGpuPreference to indicate we prefer the high-performance GPU.)

// https://docs.nvidia.com/gameworks/content/technologies/desktop/optimus.htm
extern "C" __declspec(dllexport) unsigned int NvOptimusEnablement = 0x00000001;

// https://gpuopen.com/learn/amdpowerxpressrequesthighperformance/
extern "C" __declspec(dllexport) unsigned int AmdPowerXpressRequestHighPerformance = 0x00000001;
