#include "pch.h"
#include "UploadQueue.h"

#include "GraphicsCore.h"

UploadQueue::UploadQueue(GraphicsCore& graphics)
    : m_Graphics(graphics), CommandQueue(graphics, D3D12_COMMAND_LIST_TYPE_COPY)
{
}

PendingUpload UploadQueue::AllocateResource(const D3D12_RESOURCE_DESC& resourceDescription, const std::wstring& debugName)
{
    Assert(resourceDescription.Dimension > D3D12_RESOURCE_DIMENSION_UNKNOWN && resourceDescription.Dimension <= D3D12_RESOURCE_DIMENSION_TEXTURE3D);
    Assert(debugName.length() > 0);

    // Allocate the resource
    D3D12_HEAP_PROPERTIES resourceHeapProperties = { D3D12_HEAP_TYPE_DEFAULT };
    ComPtr<ID3D12Resource> resource;
    AssertSuccess(m_Graphics.Device()->CreateCommittedResource
    (
        &resourceHeapProperties,
        D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
        &resourceDescription,
        // Since we're going to first access this resource from a copy queue, it needs to start in the common state rather than copy destintation
        // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states#constants
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&resource)
    ));
    resource->SetName(debugName.c_str());

    // Determine the layout of the upload resource
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT uploadPlacedFootprint;
    UINT rowCount;
    UINT64 rowSizeBytes;
    UINT64 uploadBufferSize;
    m_Graphics.Device()->GetCopyableFootprints(&resourceDescription, 0, 1, 0, &uploadPlacedFootprint, &rowCount, &rowSizeBytes, &uploadBufferSize);

    // Create the upload buffer
    //TODO: For resources under a certain size it'd might make sense to pool and recycle buffers
    D3D12_HEAP_PROPERTIES uploadHeapProperties = { D3D12_HEAP_TYPE_UPLOAD };
    D3D12_RESOURCE_DESC uploadResourceDescription = DescribeBufferResource(uploadBufferSize, D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);

    ComPtr<ID3D12Resource> uploadResource;
    AssertSuccess(m_Graphics.Device()->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
        &uploadResourceDescription,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadResource)
    ));
    uploadResource->SetName(std::format(L"{} (Upload)", debugName).c_str());

    // Map the upload resource
    uint8_t* mappedPtr;
    D3D12_RANGE emptyRange = { }; // We aren't going to read anything
    AssertSuccess(uploadResource->Map(0, &emptyRange, (void**)&mappedPtr));

    size_t mappedSpanSize = uploadBufferSize - uploadPlacedFootprint.Offset;
    mappedPtr += uploadPlacedFootprint.Offset;

    std::span<uint8_t> mappedSpan(mappedPtr, mappedSpanSize);

    // Return the pending upload
    Assert(rowCount < std::numeric_limits<uint32_t>::max() && "Row count can't fit in a uint!");
    Assert(rowSizeBytes < std::numeric_limits<uint32_t>::max() && "Row length can't fit in a uint!");
    return PendingUpload
    (
        *this,
        std::move(uploadResource),
        std::move(resource),
        uploadPlacedFootprint,
        resourceDescription.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER, // isTextureUpload
        mappedSpan,
        (uint32_t)rowCount,
        (uint32_t)rowSizeBytes
    );
}

InitiatedUpload UploadQueue::InitiateUpload(PendingUpload& job)
{
    // Unmap the staging buffer
    job.m_UploadResource->Unmap(0, nullptr);

    // Perform the upload
    GpuSyncPoint syncPoint;
    if (job.m_IsTextureUpload)
    { syncPoint = PerformTextureUpload(job.m_Resource.Get(), job.m_UploadResource.Get(), job.m_UploadPlacedFootprint); }
    else
    { syncPoint = PerformBufferUpload(job.m_Resource.Get(), job.m_UploadResource.Get()); }

    // Note: No resource barrier is needed or possible after uploading.
    // Resources written on copy queues must always implicitly decay to D3D12_RESOURCE_STATE_COMMON
    // https://docs.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12#state-decay-to-common
    // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states#constants

    // Queue the staging resource for cleanup
    {
        std::lock_guard<std::mutex> lock(m_CleanupQueueLock);
        m_CleanupQueue.push({ syncPoint, std::move(job.m_UploadResource) });
    }

    // All done!
    return
    {
        .Resource = std::move(job.m_Resource),
        .SyncPoint = syncPoint,
    };
}

GpuSyncPoint UploadQueue::PerformTextureUpload(ID3D12Resource* destination, ID3D12Resource* source, const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& uploadPlacedFootprint)
{
    CommandContext& context = RentContext();
    context.Begin(nullptr);

    D3D12_TEXTURE_COPY_LOCATION sourceLocation =
    {
        .pResource = source,
        .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
        .PlacedFootprint = uploadPlacedFootprint,
    };

    D3D12_TEXTURE_COPY_LOCATION destinationLocation =
    {
        .pResource = destination,
        .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .SubresourceIndex = 0,
    };

    context.m_CommandList->CopyTextureRegion(&destinationLocation, 0, 0, 0, &sourceLocation, nullptr);

    GpuSyncPoint syncPoint = context.Finish();
    ReturnContext(context);
    return syncPoint;
}

GpuSyncPoint UploadQueue::PerformBufferUpload(ID3D12Resource* destination, ID3D12Resource* source, uint64_t length)
{
    CommandContext& context = RentContext();
    context.Begin(nullptr);
    
    if (length == -1)
    {
        context.m_CommandList->CopyResource(destination, source);
    }
    else
    {
        Assert(length != 0);
        context.m_CommandList->CopyBufferRegion(destination, 0, source, 0, length);
    }

    GpuSyncPoint syncPoint = context.Finish();
    ReturnContext(context);
    return syncPoint;
}

InitiatedUpload PendingUpload::InitiateUpload()
{
    Assert(m_StagingBuffer.data() != nullptr && "Attempted to initiate the upload of job which was already submitted!");

    // The staging buffer will be invalid after initiating the upload
    // This also serves as an indicator that this upload job has been initiated
    m_StagingBuffer = { };

    // Initiate the upload, as a side-effect our resource handles will be moved out from us
    return m_UploadQueue.InitiateUpload(*this);
}

void UploadQueue::Flush()
{
    QueueSyncPoint().Wait();
    
    // At this point all outstanding upload jobs are complete, just drain the queue
    std::lock_guard<std::mutex> lock(m_CleanupQueueLock);
    while (!m_CleanupQueue.empty())
    {
        // Popping will implicitly release the resource
        m_CleanupQueue.pop();
    }
}

void UploadQueue::Cleanup()
{
    std::lock_guard<std::mutex> lock(m_CleanupQueueLock);
    while (!m_CleanupQueue.empty() && m_CleanupQueue.front().first.WasReached())
    {
        // Popping will implicitly release the resource
        m_CleanupQueue.pop();
    }
}
