#include "GpuResource.h"

#include <Windows.h>

#ifdef DEBUG
void GpuResource::TakeOwnership(CommandContext* context)
{
    CommandContext* previousOwner = (CommandContext*)InterlockedCompareExchangePointer((void**)&m_OwningContext, context, nullptr);

    // Fail if we had an owner before that wasn't the specified context.
    // (We don't care if a context takes ownership of us twice to simplify things when it transitions us more than once.)
    Assert(previousOwner == nullptr || previousOwner == context);
}

void GpuResource::ReleaseOwnership(CommandContext* context)
{
    CommandContext* previousOwner = (CommandContext*)InterlockedCompareExchangePointer((void**)&m_OwningContext, nullptr, context);

    // Fail if we had an owner and it wasn't the specified context
    // (We don't care if the context release us when we have no owner in order to simplify things when a command context transitions this resource more than once.)
    Assert(previousOwner == nullptr || previousOwner == context);
}
#endif
