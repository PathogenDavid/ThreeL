#include "pch.h"
#include "BitonicSort.h"

#include "ComputeContext.h"
#include "GpuResource.h"
#include "GraphicsContext.h"
#include "GraphicsCore.h"
#include "HlslCompiler.h"
#include "UavCounter.h"

BitonicSort::BitonicSort(GraphicsCore& graphics, HlslCompiler& hlslCompiler)
{
    // Compile all shaders
    ShaderBlobs prepareIndirectArgs = hlslCompiler.CompileShader(L"Shaders/BitonicSort/BitonicPrepareIndirectArgs.cs.hlsl", L"main", L"cs_6_0");
    ShaderBlobs preSortCombined = hlslCompiler.CompileShader(L"Shaders/BitonicSort/BitonicPreSort.cs.hlsl", L"main", L"cs_6_0");
    ShaderBlobs innerSortCombined = hlslCompiler.CompileShader(L"Shaders/BitonicSort/BitonicInnerSort.cs.hlsl", L"main", L"cs_6_0");
    ShaderBlobs outerSortCombined = hlslCompiler.CompileShader(L"Shaders/BitonicSort/BitonicOuterSort.cs.hlsl", L"main", L"cs_6_0");
    ShaderBlobs preSortSeparate = hlslCompiler.CompileShader(L"Shaders/BitonicSort/BitonicPreSort.cs.hlsl", L"main", L"cs_6_0", { L"BITONICSORT_64BIT" });
    ShaderBlobs innerSortSeparate = hlslCompiler.CompileShader(L"Shaders/BitonicSort/BitonicInnerSort.cs.hlsl", L"main", L"cs_6_0", { L"BITONICSORT_64BIT" });
    ShaderBlobs outerSortSeparate = hlslCompiler.CompileShader(L"Shaders/BitonicSort/BitonicOuterSort.cs.hlsl", L"main", L"cs_6_0", { L"BITONICSORT_64BIT" });

    // Create root signature and pipeline state objects
    m_RootSignature = RootSignature(graphics, prepareIndirectArgs, L"Bitonic Sort Root Signature");

    D3D12_COMPUTE_PIPELINE_STATE_DESC description =
    {
        .pRootSignature = m_RootSignature.Get(),
        .CS = prepareIndirectArgs.ShaderBytecode(),
    };
    m_PrepareIndirectArgs = PipelineStateObject(graphics, description, L"Bitonic Sort Prepare Indirect Args");
    description.CS = preSortCombined.ShaderBytecode();
    m_PreSortCombined = PipelineStateObject(graphics, description, L"Bitonic Pre-Sort (Combined)");
    description.CS = innerSortCombined.ShaderBytecode();
    m_InnerSortCombined = PipelineStateObject(graphics, description, L"Bitonic Inner Sort (Combined)");
    description.CS = outerSortCombined.ShaderBytecode();
    m_OuterSortCombined = PipelineStateObject(graphics, description, L"Bitonic Outer Sort (Combined)");
    description.CS = preSortSeparate.ShaderBytecode();
    m_PreSortSeparate = PipelineStateObject(graphics, description, L"Bitonic Pre-Sort (Separate)");
    description.CS = innerSortSeparate.ShaderBytecode();
    m_InnerSortSeparate = PipelineStateObject(graphics, description, L"Bitonic Inner Sort (Separate)");
    description.CS = outerSortSeparate.ShaderBytecode();
    m_OuterSortSeparate = PipelineStateObject(graphics, description, L"Bitonic Outer Sort (Separate)");

    // Create buffers for indirect arguments
    D3D12_HEAP_PROPERTIES heapProperties = { D3D12_HEAP_TYPE_DEFAULT };
    const uint32_t elementCount = 22 * 23 / 2;
    const uint32_t bufferSize = elementCount * sizeof(D3D12_DISPATCH_ARGUMENTS);
    D3D12_RESOURCE_DESC indirectArgumentsDescription = DescribeBufferResource(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    for (int i = 0; i < 2; i++)
    {
        ComPtr<ID3D12Resource> indirectArguments;
        AssertSuccess(graphics.Device()->CreateCommittedResource
        (
            &heapProperties,
            D3D12_HEAP_FLAG_CREATE_NOT_ZEROED,
            &indirectArgumentsDescription,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&indirectArguments)
        ));

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDescription =
        {
            .Format = DXGI_FORMAT_R32_TYPELESS,
            .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
            .Buffer =
            {
                .FirstElement = 0,
                .NumElements = bufferSize / sizeof(uint32_t),
                .Flags = D3D12_BUFFER_UAV_FLAG_RAW,
            },
        };
        ResourceDescriptor indirectArgumentsUav = graphics.ResourceDescriptorManager().CreateUnorderedAccessView(indirectArguments.Get(), nullptr, uavDescription);

        switch (i)
        {
            case 0:
                indirectArguments->SetName(L"Bitonic Sort Indirect Arguments (Graphics Queue)");
                m_GraphicsIndirectArgsBuffer = RawGpuResource(std::move(indirectArguments));
                m_GraphicsIndirectArgsBufferUav = indirectArgumentsUav;
                break;
            case 1:
                indirectArguments->SetName(L"Bitonic Sort Indirect Arguments (Compute Queue)");
                m_ComputeIndirectArgsBuffer = RawGpuResource(std::move(indirectArguments));
                m_ComputeIndirectArgsBufferUav = indirectArgumentsUav;
                break;
            default:
                Fail("Unreachable");
        }
    }
}

namespace BitonicShader
{
    enum RootParameters
    {
        RpGeneralArgs,
        RpCounterBuffer,
        RpSortBuffer,
        RpIndirectArgs = RpSortBuffer,
        RpSortArgs,
    };

    struct SortArgs
    {
        uint32_t CounterOffset;
        uint32_t NullItem;
    };

    struct OuterSortArgs
    {
        uint32_t k;
        uint32_t j;
    };
}

void BitonicSort::Sort(ComputeContext& context, const BitonicSortParams& params)
{
    Assert(params.Capacity > 1);

    // MiniEngine doesn't assert this in their bitonic sort dispatch, but I'm pretty sure it's requred for sorting ascending to work correctly
    // For descending the out of bounds reads will be 0, so 
    Assert(Math::IsPowerOfTwo(params.Capacity));

    uint32_t alignedCapacity = Math::AlignPowerOfTwo(params.Capacity);
    uint32_t maxIterations = Math::Log2(std::max(2048u, alignedCapacity)) - 10;

    // Select the indirect arguments buffer to use based on the command queue we'll be submitted to
    // (We need them to be separate to avoid conflcits between sorts potentially happening concurrently between async compute and graphics queues.)
    RawGpuResource& indirectArgsBuffer = context.QueueType() == D3D12_COMMAND_LIST_TYPE_COMPUTE ? m_ComputeIndirectArgsBuffer : m_GraphicsIndirectArgsBuffer;
    ResourceDescriptor& indirectArgsUav = context.QueueType() == D3D12_COMMAND_LIST_TYPE_COMPUTE ? m_ComputeIndirectArgsBufferUav : m_GraphicsIndirectArgsBufferUav;

    // Set common root signature arguments
    context->SetComputeRootSignature(m_RootSignature);
    BitonicShader::SortArgs sortArgs =
    {
        .CounterOffset = 0,
        .NullItem = params.SortAscending ? 0xFFFFFFFF : 0x00000000,
    };
    context->SetComputeRoot32BitConstants(BitonicShader::RpSortArgs, sizeof(sortArgs) / sizeof(uint32_t), &sortArgs, 0);

    // Prepare indirect dispatch arguments
    context->SetPipelineState(m_PrepareIndirectArgs);
    context.TransitionResource(params.ItemCountBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    context.TransitionResource(indirectArgsBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
    context->SetComputeRoot32BitConstant(BitonicShader::RpGeneralArgs, maxIterations, 0);
    context->SetComputeRootDescriptorTable(BitonicShader::RpCounterBuffer, params.ItemCountBuffer.Srv().ResidentHandle());
    context->SetComputeRootDescriptorTable(BitonicShader::RpIndirectArgs, indirectArgsUav.ResidentHandle());
    context.Dispatch(1);

    // Pre-sort the list up to k = 2048
    // This will also pad the list with the NullItem determined above so that the rest of the algorithm can operate without caring about the number of items used
    //TODO: I don't think the NullItem thing is actually implemented correctly.
    // I think the intent was that StoreKeyIndexPair in BitonicPreSort should've been checking the capacity of the list rather than the count.
    // Maybe I'm missing something, but isn't the idea that you can skip all the bounds checks in InnerSort/OuterSort? The MiniEngine bitonic sort still checks it all the time.
    // (If it didn't it'd end up barfing on SortAscending when the sort buffer's capacity isn't a power of two anyway -- it would be relying on out of bounds UAV accesses on tabled descriptors reading 0.)
    // Ah ha, I'm not crazy. The implementation was changed to support non-power-of-two-sized lists and in the process the null padding was broken and made useless.
    // https://github.com/microsoft/DirectX-Graphics-Samples/commit/def3a2cb9fb49f3005349a6238662729b16baf68
    // Unfortunately the old implementation has its own problems, and I'm too half awake to implement my own bitoic sort.
    // Plus I just want my particles to be sorted. Maybe some other time...
    context.TransitionResource(indirectArgsBuffer, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
    context.TransitionResource(params.SortList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    context.UavBarrier(params.SortList);
    context->SetComputeRootDescriptorTable(BitonicShader::RpSortBuffer, params.SortListUav.ResidentHandle());

    if (!params.SkipPreSort)
    {
        context->SetPipelineState(params.ItemKind == BitonicSortParams::CombinedKeyIndex ? m_PreSortCombined : m_PreSortSeparate);
        context.DispatchIndirect(indirectArgsBuffer);
        context.UavBarrier(params.SortList);
    }

    // Pre-sorting took care of swaps for k up to 2048, so now we continue at k = 4096
    // (Note that some of the outer sorts will be skipped by dispatching zero-sized groups as needed once k grows too large)
    uint32_t indirectArgsOffset = sizeof(D3D12_DISPATCH_ARGUMENTS); // Start after pre-sort args
    for (uint32_t k = 4096; k <= alignedCapacity; k *= 2)
    {
        // Outer sort iterations -- Swaps for which the distance (j) exceeds the width of the LDS and goes directly through memory
        context->SetPipelineState(params.ItemKind == BitonicSortParams::CombinedKeyIndex ? m_OuterSortCombined : m_OuterSortSeparate);
        for (uint32_t j = k / 2; j >= 2048; j /= 2)
        {
            BitonicShader::OuterSortArgs outerArgs =
            {
                .k = k,
                .j = j,
            };
            context->SetComputeRoot32BitConstants(BitonicShader::RpGeneralArgs, sizeof(outerArgs) / sizeof(uint32_t), &outerArgs, 0);
            context.DispatchIndirect(indirectArgsBuffer, indirectArgsOffset);
            indirectArgsOffset += sizeof(D3D12_DISPATCH_ARGUMENTS);
            context.UavBarrier(params.SortList);
        }

        // Inner sort iteration -- Swaps for which the distance (j) fits within LDS so looping over j occurs within the shader directly
        context->SetPipelineState(params.ItemKind == BitonicSortParams::CombinedKeyIndex ? m_InnerSortCombined : m_InnerSortSeparate);
        context.DispatchIndirect(indirectArgsBuffer, indirectArgsOffset);
        indirectArgsOffset += sizeof(D3D12_DISPATCH_ARGUMENTS);
        context.UavBarrier(params.SortList);
    }
}
