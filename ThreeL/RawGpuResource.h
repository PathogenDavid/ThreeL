#pragma once
#include "GpuResource.h"

// GpuResource is not super ideal for composite resources like LightLinkedList. This helper allows types like it to
// use interfaces which require GpuResource references when the resource in question doesn't have a suitable container
// elsewhere in ThreeL.
// A more elegant solution would probably be to provide a richer set of more basic abstractions like MiniEngine does.
// However I don't love the extra layer of abstraction, and I'm already considering revisiting the resource barrier
// handling as it is (and it's the main reason GpuResource exists in the first place.)
class RawGpuResource final : public GpuResource
{
public:
    RawGpuResource() = default;

    RawGpuResource(ComPtr<ID3D12Resource>&& resource, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON)
    {
        m_Resource = resource;
        m_CurrentState = initialState;
    }

    inline ID3D12Resource* Resource() const { return m_Resource.Get(); }
    inline operator ID3D12Resource* () const { return Resource(); }
    inline ID3D12Resource* operator->() const { return Resource(); }
};
