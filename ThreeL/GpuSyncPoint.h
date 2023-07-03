#pragma once
#include "pch.h"

//! Represents a GPU fence synchronization point.
//! This struct is free-threaded.
struct GpuSyncPoint
{
    friend class CommandQueue;

private:
    mutable ID3D12Fence* m_Fence;
    uint64_t m_FenceValue;

public:
    GpuSyncPoint()
        : m_Fence(nullptr), m_FenceValue(0)
    {
    }

    GpuSyncPoint(ID3D12Fence* fence, uint64_t fenceValue)
        : m_Fence(fence), m_FenceValue(fenceValue)
    {
    }

    inline bool WasReached() const
    {
        ID3D12Fence* fence = m_Fence;
        if (fence != nullptr)
        {
            if (fence->GetCompletedValue() < m_FenceValue)
            {
                return false;
            }

            m_Fence = nullptr;
        }

        return true;
    }

    inline void AssertReached() const
    {
#ifdef DEBUG
        ID3D12Fence* fence = m_Fence;
        if (fence != nullptr)
        {
            AssertReachedCold(fence);
        }
#endif
    }

private:
    __declspec(noinline) void AssertReachedCold(ID3D12Fence* fence) const;

public:

    inline void Wait() const
    {
        ID3D12Fence* fence = m_Fence;
        if (fence != nullptr)
        {
            if (fence->GetCompletedValue() < m_FenceValue)
            {
                AssertSuccess(fence->SetEventOnCompletion(m_FenceValue, nullptr));
            }

            m_Fence = nullptr;
        }
    }

    inline void Wait(ID3D12CommandQueue* queue) const
    {
        ID3D12Fence* fence = m_Fence;
        if (fence != nullptr)
        {
            if (fence->GetCompletedValue() >= m_FenceValue)
            {
                m_Fence = nullptr;
            }
            else
            {
                queue->Wait(fence, m_FenceValue);
            }
        }
    }

    //! Returns a GPU sync point which has already been reached.
    inline static GpuSyncPoint CreateAlreadyReached()
    {
        return GpuSyncPoint();
    }
};
