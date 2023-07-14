#pragma once
#include "CommandQueue.h"
#include "GpuSyncPoint.h"
#include "pch.h"

class GraphicsCore;
class UploadQueue;

struct InitiatedUpload
{
    ComPtr<ID3D12Resource> Resource;
    GpuSyncPoint SyncPoint;
};

struct PendingUpload
{
    friend class UploadQueue;
private:
    UploadQueue& m_UploadQueue;
    ComPtr<ID3D12Resource> m_UploadResource;
    ComPtr<ID3D12Resource> m_Resource;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT m_UploadPlacedFootprint;
    bool m_IsTextureUpload;

    std::span<uint8_t> m_StagingBuffer; // start is nullptr when upload already done
    const uint32_t m_RowCount;
    const uint32_t m_RowLengthBytes;

    PendingUpload
    (
        UploadQueue& uploadQueue,
        ComPtr<ID3D12Resource>&& uploadResource,
        ComPtr<ID3D12Resource>&& resource,
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT uploadPlacedFootprint,
        bool isTextureUpload,
        std::span<uint8_t> stagingBuffer,
        uint32_t rowCount,
        uint32_t rowLengthBytes
    )
        : m_UploadQueue(uploadQueue)
        , m_UploadResource(uploadResource)
        , m_Resource(resource)
        , m_UploadPlacedFootprint(uploadPlacedFootprint)
        , m_IsTextureUpload(isTextureUpload)
        , m_StagingBuffer(stagingBuffer)
        , m_RowCount(rowCount)
        , m_RowLengthBytes(rowLengthBytes)
    {
    }

    // Copying a pending upload is never valid and is a recipe for state corruption
    PendingUpload(const PendingUpload&) = delete;
    PendingUpload& operator =(const PendingUpload&) = delete;

public:
    inline std::span<uint8_t> StagingBuffer() { return m_StagingBuffer; }
    inline uint32_t RowCount() { return m_RowCount; }
    inline uint32_t RowLengthBytes() { return m_RowLengthBytes; }
    inline uint32_t RowPitchBytes() { return m_UploadPlacedFootprint.Footprint.RowPitch; }

    inline std::span<uint8_t> GetRow(uint32_t rowIndex)
    {
        Assert(rowIndex < m_RowCount);
        return m_StagingBuffer.subspan(rowIndex * RowPitchBytes(), m_RowLengthBytes);
    }

    InitiatedUpload InitiateUpload();

    ~PendingUpload()
    {
        Assert(m_StagingBuffer.data() == nullptr && "Pending upload was allowed to destruct before it was initiated!");
        Assert(m_Resource.Get() == nullptr);
        Assert(m_UploadResource.Get() == nullptr);
    }
};

class UploadQueue : public CommandQueue
{
    friend struct PendingUpload;
    friend class FrequentlyUpdatedResource;
private:
    GraphicsCore& m_Graphics;

    std::mutex m_CleanupQueueLock;
    std::queue<std::pair<GpuSyncPoint, ComPtr<ID3D12Resource>>> m_CleanupQueue;

public:
    UploadQueue(GraphicsCore& graphics);

    PendingUpload AllocateResource(const D3D12_RESOURCE_DESC& resourceDescription, const std::wstring& debugName);

private:
    InitiatedUpload InitiateUpload(PendingUpload& job);
    GpuSyncPoint PerformTextureUpload(ID3D12Resource* destination, ID3D12Resource* source, const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& uploadPlacedFootprint);
    GpuSyncPoint PerformBufferUpload(ID3D12Resource* destination, ID3D12Resource* source, uint64_t length = -1);

public:
    //! Flushes all outstanding uploads assocaited with this queue
    void Flush();

    //TODO: Ideally this should just happen automagically on a background thread
    //! Cleans up outstanding staging resources which are no longer needed
    void Cleanup();
};
