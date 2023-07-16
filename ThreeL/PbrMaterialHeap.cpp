#include "pch.h"
#include "PbrMaterialHeap.h"

#include "GraphicsContext.h"
#include "GraphicsCore.h"

PbrMaterialHeap::PbrMaterialHeap(GraphicsCore& graphics)
    : m_Graphics(graphics)
{
}

PbrMaterialId PbrMaterialHeap::CreateMaterial(const ShaderInterop::PbrMaterialParams& params)
{
    Assert(m_MaterialParams == nullptr && "Materials cannot be created after they've previously been uploaded!");
    m_MaterialParamsStaging.push_back(params);
    return (uint32_t)m_MaterialParamsStaging.size() - 1;
}

GpuSyncPoint PbrMaterialHeap::UploadMaterials()
{
    Assert(m_MaterialParams == nullptr && "Materials have already been uploaded!");

    // Handle edge case where there's no materials
    if (m_MaterialParamsStaging.size() == 0)
    { m_MaterialParamsStaging.push_back({ }); }

    D3D12_RESOURCE_DESC resourceDescription = DescribeBufferResource(m_MaterialParamsStaging.size() * sizeof(ShaderInterop::PbrMaterialParams));
    PendingUpload pendingUpload = m_Graphics.UploadQueue().AllocateResource(resourceDescription, L"PBR Material Parameters");

    std::span<const ShaderInterop::PbrMaterialParams> sourceSpan = m_MaterialParamsStaging;
    std::span<ShaderInterop::PbrMaterialParams> destinationSpan = SpanCast<uint8_t, ShaderInterop::PbrMaterialParams>(pendingUpload.StagingBuffer());
    SpanCopy(destinationSpan, sourceSpan);

    // CPU-side staging buffer is no longer needed
    m_MaterialParamsStaging.clear();

    // Initiate the upload
    InitiatedUpload upload = pendingUpload.InitiateUpload();
    m_MaterialParams = std::move(upload.Resource);
    return upload.SyncPoint;
}
