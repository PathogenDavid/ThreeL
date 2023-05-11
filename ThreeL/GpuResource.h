#pragma once
#include "Util.h"

#include <d3d12.h>

class CommandContext;

class GpuResource
{
    friend class CommandContext;

protected:
    ComPtr<ID3D12Resource> m_Resource;
    D3D12_RESOURCE_STATES m_CurrentState;

#ifdef DEBUG
private:
    //! This keeps track of which context "owns" this resource.
    //! A context owns a resource as soon as it transitions it and it is released when the resource barriers are explicitly flushed or some draw/execute operation is done.
    //! 
    //! This exists only as a debugging aid. It is by no means perfect or absolute.
    CommandContext* m_OwningContext;
    void TakeOwnership(CommandContext* context);
    void ReleaseOwnership(CommandContext* context);
#endif
};
