#include "pch.h"
#include "ComputeContext.h"

#include "CommandQueue.h"

ComputeContext::ComputeContext(CommandQueue& queue)
{
    Assert(queue.m_Type == D3D12_COMMAND_LIST_TYPE_COMPUTE || queue.m_Type == D3D12_COMMAND_LIST_TYPE_DIRECT);
    m_Context = &queue.RentContext();
    m_Context->Begin(nullptr);
}

ComputeContext::ComputeContext(CommandQueue& queue, ID3D12RootSignature* rootSignature, ID3D12PipelineState* pipelineState)
{
    Assert(queue.m_Type == D3D12_COMMAND_LIST_TYPE_COMPUTE || queue.m_Type == D3D12_COMMAND_LIST_TYPE_DIRECT);
    Assert(rootSignature != nullptr);
    Assert(pipelineState != nullptr);
    m_Context = &queue.RentContext();
    m_Context->Begin(pipelineState);
    m_Context->m_CommandList->SetComputeRootSignature(rootSignature);
}

void ComputeContext::Dispatch(uint3 threadGroupCount)
{
    m_Context->FlushResourceBarriers();
    m_Context->m_CommandList->Dispatch(threadGroupCount.x, threadGroupCount.y, threadGroupCount.z);
}

GpuSyncPoint ComputeContext::Finish()
{
    GpuSyncPoint syncPoint = m_Context->Finish();
    m_Context->m_CommandQueue.ReturnContext(*m_Context);
    m_Context = nullptr;
    return syncPoint;
}
