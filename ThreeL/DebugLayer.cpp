#include "pch.h"
#include "DebugLayer.h"

#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <mutex>

//=================================================================================================================================================//
//                                    ______               ___ __                          __   __                                                 //
//                                   |      |.-----.-----.'  _|__|.-----.--.--.----.---.-.|  |_|__|.-----.-----.                                   //
//                                   |   ---||  _  |     |   _|  ||  _  |  |  |   _|  _  ||   _|  ||  _  |     |                                   //
//                                   |______||_____|__|__|__| |__||___  |_____|__| |___._||____|__||_____|__|__|                                   //
//                                                                |_____|                                                                          //
//=================================================================================================================================================//

// This is enabled by default
// It is known to sometimes cause major performance issues on Present when mixed-mode debugging is being used,
// so consider disabling if you're having perf issues
//
// !!! Disabling this is currently broken in the Agility SDK (you'll get a crash in ExecuteCommandLists), so just leave it on !!!
// (Also this toggle might actually just be removed soon.)
// https://discord.com/channels/590611987420020747/590965902564917258/1097908429160452137
static const bool kEnableSynchronizedCommandQueueValidation = true;

// This is disabled by default
// It has a pretty major performance impact, not recommended for general use
// Enabling this requires synchronized command queue validation.
static const bool kEnableGPUBasedValidation = false;

static const D3D12_MESSAGE_SEVERITY kDeniedSeverities[] =
{
    //D3D12_MESSAGE_SEVERITY_INFO,
    (D3D12_MESSAGE_SEVERITY)-1
};

static const D3D12_MESSAGE_CATEGORY kDeniedCategories[] =
{
    (D3D12_MESSAGE_CATEGORY)-1
};

static const D3D12_MESSAGE_ID kDeniedMessages[] =
{
    D3D12_MESSAGE_ID_CREATE_COMMANDQUEUE,
    D3D12_MESSAGE_ID_CREATE_COMMANDALLOCATOR,
    D3D12_MESSAGE_ID_CREATE_COMMANDSIGNATURE,
    D3D12_MESSAGE_ID_CREATE_PIPELINESTATE,
    D3D12_MESSAGE_ID_CREATE_COMMANDLIST12,
    D3D12_MESSAGE_ID_CREATE_RESOURCE,
    D3D12_MESSAGE_ID_CREATE_DESCRIPTORHEAP,
    D3D12_MESSAGE_ID_CREATE_ROOTSIGNATURE,
    D3D12_MESSAGE_ID_CREATE_HEAP,
    D3D12_MESSAGE_ID_CREATE_MONITOREDFENCE,
    D3D12_MESSAGE_ID_CREATE_LIFETIMETRACKER,
    D3D12_MESSAGE_ID_CREATE_SHADERCACHESESSION,
    D3D12_MESSAGE_ID_CREATE_QUERYHEAP,
    D3D12_MESSAGE_ID_DESTROY_COMMANDQUEUE,
    D3D12_MESSAGE_ID_DESTROY_COMMANDALLOCATOR,
    D3D12_MESSAGE_ID_DESTROY_COMMANDSIGNATURE,
    D3D12_MESSAGE_ID_DESTROY_PIPELINESTATE,
    D3D12_MESSAGE_ID_DESTROY_COMMANDLIST12,
    D3D12_MESSAGE_ID_DESTROY_RESOURCE,
    D3D12_MESSAGE_ID_DESTROY_DESCRIPTORHEAP,
    D3D12_MESSAGE_ID_DESTROY_ROOTSIGNATURE,
    D3D12_MESSAGE_ID_DESTROY_HEAP,
    D3D12_MESSAGE_ID_DESTROY_MONITOREDFENCE,
    D3D12_MESSAGE_ID_DESTROY_LIFETIMETRACKER,
    D3D12_MESSAGE_ID_DESTROY_SHADERCACHESESSION,
    D3D12_MESSAGE_ID_DESTROY_QUERYHEAP,
    (D3D12_MESSAGE_ID)-1
};

//=================================================================================================================================================//

namespace DebugLayer
{
    static bool g_IsEnabled = false;
    static bool g_IsGpuBasedValidationEnabled = false;

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

        // Set allow-all storage filter and ensure message retrieval is disabled
        AssertSuccess(dxgiInfoQueue->PushEmptyStorageFilter(DXGI_DEBUG_ALL));
        AssertSuccess(dxgiInfoQueue->PushDenyAllRetrievalFilter(DXGI_DEBUG_ALL));

        // Enable break on corruption/error severities (disabled for everything else.)
        AssertSuccess(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true));
        AssertSuccess(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true));
        AssertSuccess(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, false));
        AssertSuccess(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_INFO, false));
        AssertSuccess(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_MESSAGE, false));

        // Suppress select D3D debug layer messages
        // We do this as the DXGI level (IE: before the device is even initialized) allows us to suppress messages that are emitted by D3D12 internals
        DXGI_INFO_QUEUE_FILTER filter =
        {
            .DenyList =
            {
                .NumCategories = (UINT)std::size(kDeniedCategories) - 1,
                .pCategoryList = (DXGI_INFO_QUEUE_MESSAGE_CATEGORY*)kDeniedCategories,
                .NumSeverities = (UINT)std::size(kDeniedSeverities) - 1,
                .pSeverityList = (DXGI_INFO_QUEUE_MESSAGE_SEVERITY*)kDeniedSeverities,
                .NumIDs = (UINT)std::size(kDeniedMessages) - 1,
                .pIDList = (DXGI_INFO_QUEUE_MESSAGE_ID*)kDeniedMessages,
            },
        };
        AssertSuccess(dxgiInfoQueue->PushStorageFilter(DXGI_DEBUG_D3D12, &filter));

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
            debugController1->SetEnableSynchronizedCommandQueueValidation(kEnableSynchronizedCommandQueueValidation);

            // Enabling this requires synchronized command queue validation.
            g_IsGpuBasedValidationEnabled = kEnableGPUBasedValidation && kEnableSynchronizedCommandQueueValidation;
            debugController1->SetEnableGPUBasedValidation(g_IsGpuBasedValidationEnabled);

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

    bool IsEnabled() { return g_IsEnabled; }
    bool IsGpuBasedValidationEnabled() { return g_IsGpuBasedValidationEnabled; }

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

    std::wstring GetExtraWindowTitleInfo()
    {
        std::wstring title = L"";

#if defined(DEBUG)
        title += L"Debug";
#elif !defined(NDEBUG)
        title += L"Checked";
#endif

        if (DebugLayer::IsEnabled())
        {
            if (title.size() > 0) { title += L"/"; }
            title += L"DebugLayer";

            if (DebugLayer::IsGpuBasedValidationEnabled())
            { title += L"+GBV"; }
        }

        if (GetModuleHandleW(L"WinPixGpuCapturer.dll") != NULL)
        {
            if (title.size() > 0) { title += L"/"; }
            title += L"PIX";
        }

        return title.size() == 0 ? title : L" (" + title + L")";
    }
}
