#pragma once
#include "pch.h"

#include "GpuSyncPoint.h"
#include "SwapChain.h"

class GraphicsCore;

//! Represents a GPU resource which is frequently updated, at most once per frame
//! The resource on the GPU will be D3D12_HEAP_TYPE_DEFAULT, making it suitable for frequent access on the GPU
class FrequentlyUpdatedResource
{
public:
    static const uint32_t RESOURCE_COUNT = SwapChain::BACK_BUFFER_COUNT;
private:
    UploadQueue& m_UploadQueue;
    ComPtr<ID3D12Resource> m_UploadResources[RESOURCE_COUNT];
    ComPtr<ID3D12Resource> m_Resources[RESOURCE_COUNT];
#ifdef DEBUG
    GpuSyncPoint m_LastUploadSyncPoints[RESOURCE_COUNT];
#endif
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT m_UploadPlacedFootprint;
    uint64_t m_UploadBufferSize;
    bool m_IsTextureUpload;
    uint32_t m_CurrentResource = RESOURCE_COUNT - 1;

public:
    FrequentlyUpdatedResource(GraphicsCore& graphics, const D3D12_RESOURCE_DESC& resourceDescription, const std::wstring& debugName);

    //! Advances to the next GPU resource and updates it with the specified data
    //! Must not be called more than once per frame
    //!
    //! For non-texture resources, `data` may be smaller than the actual buffer
    //! However the rest of the buffer's contents will be undefined
    GpuSyncPoint Update(const std::span<const uint8_t> data);

    //! Advances to the next GPU resource and updates it with the specified data
    //! Must not be called more than once per frame
    //!
    //! For non-texture resources, `data` may be smaller than the actual buffer
    //! However the rest of the buffer's contents will be undefined
    template<typename T>
    GpuSyncPoint Update(const std::span<const T> data)
    { return Update(SpanCast<const T, const uint8_t>(data)); }

    inline ID3D12Resource* Resource(uint32_t resourceIndex) const
    {
        Assert(resourceIndex < RESOURCE_COUNT);
        return m_Resources[resourceIndex].Get();
    }

    inline ID3D12Resource* Current() const { return m_Resources[m_CurrentResource].Get(); }
    inline uint32_t CurrentIndex() const { return m_CurrentResource; }
};
