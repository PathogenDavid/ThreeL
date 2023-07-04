#include "pch.h"
#include "MeshHeap.h"

#include "GraphicsContext.h"

// The mesh heap allocates chunks as needed to fit mesh data, each chunk uses the size defined below
// It should always be a multiple of 64k (D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT) otherwise you're just wasting space on padding
// Note that we don't currently have a fallback for mesh data which fails to fit in a single chunk, so setting this too low would cause issues for complex meshes
#define CHUNK_SIZE (D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT * 16)

MeshHeap::MeshHeap(GraphicsCore& graphics)
    : m_Graphics(graphics)
{
    m_StagingBuffer = AllocateChunk(true);
    D3D12_RANGE readRange = { };
    AssertSuccess(m_StagingBuffer->Map(0, &readRange, (void**)&m_MappedStagingBuffer));

    m_CurrentBuffer = nullptr;
    m_CurrentOffset = 0;
    m_CurrentGpuBaseAddress = { };
}

ComPtr<ID3D12Resource> MeshHeap::AllocateChunk(bool isStagingBuffer)
{
    D3D12_RESOURCE_DESC description =
    {
        .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
        .Alignment = 0,
        .Width = CHUNK_SIZE,
        .Height = 1,
        .DepthOrArraySize = 1,
        .MipLevels = 1,
        .Format = DXGI_FORMAT_UNKNOWN,
        .SampleDesc = {.Count = 1, .Quality = 0 },
        .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
        .Flags = isStagingBuffer ? D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE : D3D12_RESOURCE_FLAG_NONE,
    };

    D3D12_HEAP_PROPERTIES heapProperties =
    {
        .Type = isStagingBuffer ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT,
    };

    ComPtr<ID3D12Resource> newChunk;
    AssertSuccess(m_Graphics.GetDevice()->CreateCommittedResource
    (
        &heapProperties,
        D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
        &description,
        isStagingBuffer ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&newChunk)
    ));

    if (isStagingBuffer)
    {
        newChunk->SetName(L"MeshHeap CPU staging buffer");
    }
    else
    {
        newChunk->SetName(std::format(L"MeshHeap GPU buffer #{}", m_MeshBuffers.size()).c_str());
    }

    return newChunk;
}

D3D12_GPU_VIRTUAL_ADDRESS MeshHeap::Allocate(const void* buffer, size_t sizeBytes)
{
    Assert(sizeBytes <= CHUNK_SIZE && "Allocation failed: Mesh data exceeds the chunk size.");

    // Flush the current buffer if the new data won't fit
    if (m_CurrentBuffer != nullptr)
    {
        size_t remainingBytes = CHUNK_SIZE - m_CurrentOffset;
        if (remainingBytes < sizeBytes)
        {
            Flush();
            Assert(m_CurrentBuffer == nullptr);
        }
    }

    // Allocate a new chunk if we don't currently have one
    if (m_CurrentBuffer == nullptr)
    {
        ComPtr<ID3D12Resource> newChunk = AllocateChunk(false);
        m_CurrentBuffer = newChunk.Get();
        m_CurrentOffset = 0;
        m_CurrentGpuBaseAddress = newChunk->GetGPUVirtualAddress();
        m_MeshBuffers.push_back(std::move(newChunk));
    }

    // Copy data into the staging buffer
    memcpy(m_MappedStagingBuffer + m_CurrentOffset, buffer, sizeBytes);

    // Calculate the (future) GPU address of this buffer and update the offset for the next buffer (ensuring it's going to be aligned on a 4 byte boundary
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = m_CurrentGpuBaseAddress + m_CurrentOffset;
    m_CurrentOffset += sizeBytes + (sizeBytes % 4);
    return gpuAddress;
}

void MeshHeap::Flush()
{
    // Do nothing if staging buffer contains no pending data
    if (m_CurrentOffset == 0)
    {
        return;
    }

    // Upload staged data to the GPU buffer
    m_StagingBuffer->Unmap(0, nullptr);

    GraphicsContext context(m_Graphics.GetGraphicsQueue());
    context->CopyResource(m_CurrentBuffer, m_StagingBuffer.Get());

    // Note: This explicit barrier is not needed (or possible) once we adapt this class to use the upload queue
    D3D12_RESOURCE_BARRIER barrier =
    {
        .Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        .Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
        .Transition =
        {
            .pResource = m_CurrentBuffer,
            .Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
            .StateBefore = D3D12_RESOURCE_STATE_COPY_DEST,
            .StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER | D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        },
    };
    context->ResourceBarrier(1, &barrier);

    context.Finish().Wait(); //TODO: Ideally we'd rent a new staging buffer and keep working while the GPU copies

    // Clear fields relating to the current GPU chunk since this one is now finished
    m_CurrentBuffer = nullptr;
    m_CurrentOffset = 0;
    m_CurrentGpuBaseAddress = { };

    // Re-map the staging buffer
    D3D12_RANGE readRange = { };
    AssertSuccess(m_StagingBuffer->Map(0, &readRange, (void**)&m_MappedStagingBuffer));
}
