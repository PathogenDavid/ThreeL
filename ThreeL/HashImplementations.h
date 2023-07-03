#pragma once
#include <d3d12.h>
#include <functional>
#include <xxhash.h>

//=====================================================================================================================
// D3D12_SAMPLER_DESC
//=====================================================================================================================
static_assert(sizeof(std::size_t) == sizeof(uint64_t), "These implementations assume size_t is 64 bits.");
static_assert
(
    sizeof(D3D12_SAMPLER_DESC) == sizeof(D3D12_FILTER) + sizeof(D3D12_TEXTURE_ADDRESS_MODE) * 3 + sizeof(FLOAT) + sizeof(UINT) + sizeof(D3D12_COMPARISON_FUNC) + sizeof(FLOAT) * 6,
    "These implementations assume D3D12_SAMPLER_DESC has no padding."
);

template<>
struct std::hash<D3D12_SAMPLER_DESC>
{
    std::size_t operator()(const D3D12_SAMPLER_DESC& k) const noexcept { return XXH64(&k, sizeof(k), 0); }
};

inline bool operator==(const D3D12_SAMPLER_DESC& a, const D3D12_SAMPLER_DESC& b) { return memcmp(&a, &b, sizeof(D3D12_SAMPLER_DESC)) == 0; }

//=====================================================================================================================
// std::pair<T1, T2>
//=====================================================================================================================
template<typename T1, typename T2>
struct std::hash<std::pair<T1, T2>>
{
    std::size_t operator()(const std::pair<T1, T2>& k) const noexcept
    {
        std::size_t parts[] = { std::hash<T1>{}(k.first), std::hash<T2>{}(k.second) };
        return XXH64(parts, sizeof(parts), 0);
    }
};
