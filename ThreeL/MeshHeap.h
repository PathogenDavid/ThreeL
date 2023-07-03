#pragma once
#include "pch.h"
#include "GraphicsCore.h"

//TODO: Adapt this to use UploadHeap
class MeshHeap
{
private:
    GraphicsCore& m_Graphics;

    ComPtr<ID3D12Resource> m_StagingBuffer;
    uint8_t* m_MappedStagingBuffer;

    std::vector<ComPtr<ID3D12Resource>> m_MeshBuffers;

    ID3D12Resource* m_CurrentBuffer;
    size_t m_CurrentOffset;
    D3D12_GPU_VIRTUAL_ADDRESS m_CurrentGpuBaseAddress;

public:
    MeshHeap(GraphicsCore& graphics);

private:
    ComPtr<ID3D12Resource> AllocateChunk(bool isStagingBuffer);

    D3D12_GPU_VIRTUAL_ADDRESS Allocate(const void* buffer, size_t sizeBytes);

public:
    inline D3D12_INDEX_BUFFER_VIEW AllocateIndexBuffer(const void* rawIndices, size_t sizeBytes, DXGI_FORMAT format)
    {
        return {
            .BufferLocation = Allocate(rawIndices, sizeBytes),
            .SizeInBytes = (UINT)sizeBytes,
            .Format = format,
        };
    }

    inline D3D12_INDEX_BUFFER_VIEW AllocateIndexBuffer(std::span<const uint16_t> indices)
    {
        return AllocateIndexBuffer(indices.data(), indices.size_bytes(), DXGI_FORMAT_R16_UINT);
    }

    inline D3D12_INDEX_BUFFER_VIEW AllocateIndexBuffer(std::span<const uint32_t> indices)
    {
        return AllocateIndexBuffer(indices.data(), indices.size_bytes(), DXGI_FORMAT_R32_UINT);
    }

    inline D3D12_VERTEX_BUFFER_VIEW AllocateVertexBuffer(const void* rawVertexData, size_t sizeBytes, uint32_t strideBytes)
    {
        return {
            .BufferLocation = Allocate(rawVertexData, sizeBytes),
            .SizeInBytes = (UINT)sizeBytes,
            .StrideInBytes = strideBytes,
        };
    }

    template<typename T>
    inline D3D12_VERTEX_BUFFER_VIEW AllocateVertexBuffer(std::span<const T> vertexData)
    {
        return AllocateVertexBuffer(vertexData.data(), vertexData.size_bytes(), sizeof(T));
    }

    void Flush();
};
