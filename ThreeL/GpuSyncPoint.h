#pragma once
#include "pch.h"

//! Represents a GPU fence synchronization point.
//! This struct is free-threaded.
struct GpuSyncPoint
{
    friend class CommandQueue;

private:
    // In the interest of making sync points as lighweight as possible, this is not a ComPtr
    // Realistically these fences live as long as the entire program, so it's pretty unlikely a sync point will be used after they're destroyed
    ID3D12Fence* m_Fence;
    uint64_t m_FenceValue;

public:
    GpuSyncPoint(ID3D12Fence* fence, uint64_t fenceValue)
        : m_Fence(fence), m_FenceValue(fenceValue)
    {
    }

    inline bool WasReached()
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

    inline void AssertReached()
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
    __declspec(noinline) void AssertReachedCold(ID3D12Fence* fence);
public:

    inline void Wait()
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

    inline void Wait(ID3D12CommandQueue* queue)
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
        return { nullptr, 0 };
    }
};
