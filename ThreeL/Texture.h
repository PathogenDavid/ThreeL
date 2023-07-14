#pragma once
#include "GpuResource.h"
#include "GpuSyncPoint.h"
#include "Math.h"
#include "ResourceDescriptor.h"

#include <d3d12.h>
#include <span>

struct ResourceManager;

class Texture : public GpuResource
{
private:
    ResourceDescriptor m_SrvHandle;
    uint32_t m_BindlessIndex;
    GpuSyncPoint m_UploadSyncPoint;

    Texture(const ResourceManager& resources, std::wstring debugName, std::span<const uint8_t> colorData, uint2 size, int texelSize, DXGI_FORMAT format);
public:
    Texture(const ResourceManager& resources, std::wstring debugName, std::span<const uint8_t> rgbaData, uint2 size, bool isSrgb = false)
        : Texture(resources, debugName, rgbaData, size, 4, isSrgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM)
    { }

private:
    inline DXGI_FORMAT GetHdrFormat(int channelCount)
    {
        switch (channelCount)
        {
            case 1: return DXGI_FORMAT_R32_FLOAT;
            case 2: return DXGI_FORMAT_R32G32_FLOAT;
            case 3: return DXGI_FORMAT_R32G32B32_FLOAT;
            case 4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
            default:
                Assert(false && "Bad channel count");
                return DXGI_FORMAT_UNKNOWN;
        }
    }

public:
    Texture(const ResourceManager& resources, std::wstring debugName, std::span<const float> colorData, uint2 size, int channelCount)
        : Texture(resources, debugName, SpanCast<const float, const uint8_t>(colorData), size, channelCount * 4, GetHdrFormat(channelCount))
    { }

    inline ResourceDescriptor SrvHandle() const { return m_SrvHandle; }
    inline uint32_t BindlessIndex() const { return m_BindlessIndex; }

    inline void WaitForUpload() const
    {
        m_UploadSyncPoint.Wait();
    }

    inline void AssertIsUploaded() const
    {
#if DEBUG
        m_UploadSyncPoint.AssertReached();
#endif
    }
};
