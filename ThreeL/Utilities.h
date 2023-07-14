#pragma once
#include <d3d12.h>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <Windows.h>

void SetWorkingDirectoryToAppDirectory();

std::wstring GetD3DObjectName(ID3D12Object* object);

std::wstring DescribeResourceState(D3D12_RESOURCE_STATES states);

template<typename TFrom, typename TTo>
inline std::span<TTo> SpanCast(std::span<TFrom> span)
{
    size_t toSize = sizeof(TFrom) == sizeof(TTo) ? span.size() : span.size_bytes() / sizeof(TTo);
    return std::span<TTo>(reinterpret_cast<TTo*>(span.data()), toSize);
}

template<typename T>
inline void SpanCopy(std::span<T> destination, std::span<const T> source)
{
    Assert(destination.size_bytes() >= source.size_bytes());
    memcpy(destination.data(), source.data(), source.size_bytes());
}

//! Implementation of std::formatter to automatically widen std::string to std::wstring, assumes std::string stores UTF8 characters.
template <>
struct std::formatter<std::string, wchar_t> {
    template<class ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <class FormatContext>
    FormatContext::iterator format(const std::string& str, FormatContext& ctx)
    {
        if (str.length() == 0) { return ctx.out(); }

        Assert(str.length() <= std::numeric_limits<int>::max());

        int requiredSize = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), nullptr, 0);
        AssertWinError(requiredSize > 0);

        std::wstring widened;
        widened.resize((size_t)requiredSize);
        Assert(widened.capacity() <= std::numeric_limits<int>::max());

        int encodedSize = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), widened.data(), (int)widened.capacity());
        AssertWinError(encodedSize > 0);
        Assert(encodedSize == requiredSize);

        return std::format_to(ctx.out(), L"{}", widened);
    }
};
