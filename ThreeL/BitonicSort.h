#pragma once
#include "RawGpuResource.h"
#include "ResourceDescriptor.h"
#include "RootSignature.h"
#include "PipelineStateObject.h"

struct ComputeContext;
class GpuResource;
struct GraphicsContext;
class GraphicsCore;
class HlslCompiler;
class UavCounter;

struct BitonicSortParams
{
    enum SortItemKind
    {
        //! The sort key and index have been combined into a single 32-bit value, with the sort key in the upper bits
        //! This format results in better sort performance at the expense of smaller indices
        CombinedKeyIndex,
        //! The sort key and index are separated in a pair of 32-bit values, with the index in X and the sort key in Y
        //! This format uses more memory bandwidth and thus results in slower sorting, but you get the full 32-bit range for the indices
        SeparateKeyIndex,
    };

    //! The list to be sorted
    GpuResource& SortList;
    ResourceDescriptor SortListUav;
    //! The maximum number of items that SortList can hold
    uint32_t Capacity;
    //! The format of the elements within SortList
    SortItemKind ItemKind;
    //! A UavCounter specifying the number of valid entries in SortList
    UavCounter& ItemCountBuffer;

    //! If true, the pre-sorting phase will be skipped.
    //! The caller asserts that the buffer is already partially sorted in blocks of 2048, meaning that the pre-sorting phase can be skipped.
    //! (This might be the case if the sort list was built in chunks of groupshared memory.)
    bool SkipPreSort;

    bool SortAscending;
};

class BitonicSort
{
private:
    RootSignature m_RootSignature;
    
    PipelineStateObject m_PrepareIndirectArgs;

    PipelineStateObject m_PreSortCombined;
    PipelineStateObject m_InnerSortCombined;
    PipelineStateObject m_OuterSortCombined;

    PipelineStateObject m_PreSortSeparate;
    PipelineStateObject m_InnerSortSeparate;
    PipelineStateObject m_OuterSortSeparate;

    RawGpuResource m_GraphicsIndirectArgsBuffer;
    ResourceDescriptor m_GraphicsIndirectArgsBufferUav;
    RawGpuResource m_ComputeIndirectArgsBuffer;
    ResourceDescriptor m_ComputeIndirectArgsBufferUav;

public:
    BitonicSort() = default;
    BitonicSort(GraphicsCore& graphics, HlslCompiler& hlslCompiler);

    void Sort(ComputeContext& context, const BitonicSortParams& params);
};
