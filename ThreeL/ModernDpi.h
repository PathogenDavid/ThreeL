#pragma once
#include <Windows.h>

namespace ModernDpi
{
    bool IsAvailable();
    bool IsEnableNonClientDpiScalingAvailable();

    typedef DPI_AWARENESS_CONTEXT WINAPI SetThreadDpiAwarenessContextFn(DPI_AWARENESS_CONTEXT dpiContext);
    extern SetThreadDpiAwarenessContextFn* SetThreadDpiAwarenessContext;
    typedef DPI_AWARENESS_CONTEXT WINAPI GetThreadDpiAwarenessContextFn(VOID);
    extern GetThreadDpiAwarenessContextFn* GetThreadDpiAwarenessContext;
    typedef DPI_AWARENESS_CONTEXT WINAPI GetWindowDpiAwarenessContextFn(HWND hwnd);
    extern GetWindowDpiAwarenessContextFn* GetWindowDpiAwarenessContext;
    typedef DPI_AWARENESS WINAPI GetAwarenessFromDpiAwarenessContextFn(_In_ DPI_AWARENESS_CONTEXT value);
    extern GetAwarenessFromDpiAwarenessContextFn* GetAwarenessFromDpiAwarenessContext;
    typedef UINT WINAPI GetDpiForWindowFn(HWND hwnd);
    extern GetDpiForWindowFn* GetDpiForWindow;
    typedef BOOL WINAPI EnableNonClientDpiScalingFn(HWND hwnd);
    extern EnableNonClientDpiScalingFn* EnableNonClientDpiScaling;

    typedef BOOL WINAPI AdjustWindowRectExForDpiFn(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle, UINT dpi);
    extern AdjustWindowRectExForDpiFn* AdjustWindowRectExForDpi;
}
