#include "DebugLayer.h"

#include "Util.h"

#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <mutex>
#include <stdio.h>

namespace DebugLayer
{
    static bool g_IsEnabled = false;

    void Setup()
    {
        static bool calledOnce = false;

        if (calledOnce)
            return;

        calledOnce = true;

        //---------------------------------------------------------------------------------------------------------
        // Configure the DXGI Info Queue
        //---------------------------------------------------------------------------------------------------------

        // Fetch and initialize the DXGI info queue
        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (FAILED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue))))
        {
            OutputDebugStringW(L"Failed to get DXGI info queue. Debug layer will not be enabled.\n");
            g_IsEnabled = false;
            return;
        }

        // Disable storage and retrieval filters
        AssertSuccess(dxgiInfoQueue->PushEmptyStorageFilter(DXGI_DEBUG_ALL));
        AssertSuccess(dxgiInfoQueue->PushEmptyRetrievalFilter(DXGI_DEBUG_ALL));

        // Enable break on corruption/error severities (disabled for everything else.)
        AssertSuccess(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true));
        AssertSuccess(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true));
        AssertSuccess(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, false));
        AssertSuccess(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_INFO, false));
        AssertSuccess(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_MESSAGE, false));

        //---------------------------------------------------------------------------------------------------------
        // Enable debug layer
        //---------------------------------------------------------------------------------------------------------
        // We don't have experimental features to enable, but this works around an annoying issue where ID3D12Device::SetStablePowerState
        // doesn't work in Windows 10 20H2 with a bogus error about enabling developer mode (even if it's already enabled.)
        // Workaround "documented" here: https://discord.com/channels/590611987420020747/590965930658496690/811575822640742451
        D3D12EnableExperimentalFeatures(0, nullptr, nullptr, nullptr);

        // Try to get the debug control interface
        ComPtr<ID3D12Debug> debugController;
        if (!SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            printf("Could not enable Direct3D debug layer. (Graphics tools not installed?)\n");
            return;
        }

        debugController->EnableDebugLayer();
        g_IsEnabled = true;

        // Configure GPU-based validation
        // https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-d3d12-debug-layer-gpu-based-validation
        ComPtr<ID3D12Debug1> debugController1;
        if (SUCCEEDED(debugController->QueryInterface(IID_PPV_ARGS(&debugController1))))
        {
            // This is enabled by default
            // It is known to sometimes cause major performance issues on Present when mixed-mode debugging is being used, so consider disabling if you're having perf issues
            debugController1->SetEnableSynchronizedCommandQueueValidation(true);

            // This is disabled by default and only left here as a placeholder.
            // Enabling this requires synchronized command queue validation.
            debugController1->SetEnableGPUBasedValidation(false);

            ComPtr<ID3D12Debug2> debugController2;
            if (SUCCEEDED(debugController1->QueryInterface(IID_PPV_ARGS(&debugController2))))
            {
                debugController2->SetGPUBasedValidationFlags(D3D12_GPU_BASED_VALIDATION_FLAGS_NONE);
            }
        }
    }

    void Configure(ID3D12Device* device)
    {
        if (!g_IsEnabled)
            return;

        // Configure message severity debug breakpoints
        ComPtr<ID3D12InfoQueue> infoQueue;
        if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
        {
            // Enable break on corruption/error severities (disabled for everything else.)
            AssertSuccess(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true));
            AssertSuccess(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true));
            AssertSuccess(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false));
            AssertSuccess(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_INFO, false));
            AssertSuccess(infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_MESSAGE, false));
        }
    }

    bool IsEnabled()
    {
        return g_IsEnabled;
    }

    void ReportLiveObjects()
    {
        ComPtr<IDXGIDebug> dxgiDebug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
        {
            OutputDebugStringW(L"===============================================================================\n");
            OutputDebugStringW(L"Living DirectX object report\n");
            OutputDebugStringW(L"===============================================================================\n");
            AssertSuccess(dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, (DXGI_DEBUG_RLO_FLAGS)(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL)));
            OutputDebugStringW(L"===============================================================================\n");
        }
    }
}
