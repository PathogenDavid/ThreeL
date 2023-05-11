#include "pch.h"
#include "GraphicsContext.h"

#include "CommandQueue.h"

GraphicsContext::GraphicsContext(GraphicsQueue& queue)
{
    Assert(queue.m_Type == D3D12_COMMAND_LIST_TYPE_DIRECT);
    m_Context = &queue.RentContext();
    m_Context->Begin(nullptr);
}

GraphicsContext::GraphicsContext(GraphicsQueue& queue, ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState)
{
    Assert(queue.m_Type == D3D12_COMMAND_LIST_TYPE_DIRECT);
    Assert(rootSignature != nullptr);
    Assert(pipelineState != nullptr);
    m_Context = &queue.RentContext();
    m_Context->Begin(pipelineState);
    m_Context->m_CommandList->SetGraphicsRootSignature(rootSignature);
}

GpuSyncPoint GraphicsContext::Finish()
{
    GpuSyncPoint syncPoint = m_Context->Finish();
    m_Context->m_CommandQueue.ReturnContext(*m_Context);
    m_Context = nullptr;
    return syncPoint;
}
