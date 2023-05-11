#pragma once
#include "CommandContext.h"
#include "RenderTargetView.h"
#include "SwapChain.h"

class GraphicsQueue;

struct GraphicsContext
{
private:
    CommandContext* m_Context;

public:
    inline ID3D12GraphicsCommandList* GetCommandList()
    {
        return m_Context->m_CommandList.Get();
    }

    GraphicsContext(GraphicsQueue& queue);
    GraphicsContext(GraphicsQueue& queue, ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState);
    GraphicsContext(const GraphicsContext&) = delete;

    inline void TransitionResource(GpuResource& resource, D3D12_RESOURCE_STATES desiredState, bool immediate = false)
    {
        m_Context->TransitionResource(resource, desiredState, immediate);
    }

    inline void TransitionResource(SwapChain& swapChain, D3D12_RESOURCE_STATES desiredState, bool immediate = false)
    {
        m_Context->TransitionResource(swapChain.CurrentBackBuffer(), desiredState, immediate);
    }

    inline void Clear(RenderTargetView renderTarget, float r, float g, float b, float a)
    {
        float color[] = { r, g, b, a };
        GetCommandList()->ClearRenderTargetView(renderTarget.GetRtvHandle(), color, 0, nullptr);
    }

    inline void SetRenderTarget(RenderTargetView renderTarget)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = renderTarget.GetRtvHandle();
        GetCommandList()->OMSetRenderTargets(1, &handle, false, nullptr);
    }

    inline GpuSyncPoint Flush(ID3D12PipelineState* newState)
    {
        m_Context->Flush(newState);
    }

    GpuSyncPoint Finish();

    ~GraphicsContext()
    {
        if (m_Context != nullptr)
        {
            Assert(false && "A GraphicsContext was allowed to go out of scope before being submitted!");
            Finish();
        }
    }
};
