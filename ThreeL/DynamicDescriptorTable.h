#pragma once
#include <d3d12.h>

struct DynamicDescriptorTable
{
    friend struct DynamicDescriptorTableBuilder;

private:
    const D3D12_GPU_DESCRIPTOR_HANDLE m_Handle;

    DynamicDescriptorTable(D3D12_GPU_DESCRIPTOR_HANDLE handle)
        : m_Handle(handle)
    {
    }
};
