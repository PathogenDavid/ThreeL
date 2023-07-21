#include "pch.h"
#include "ModernDpi.h"

namespace ModernDpi
{
    static HMODULE User32 = ::GetModuleHandleW(L"USER32.DLL");
    SetThreadDpiAwarenessContextFn* SetThreadDpiAwarenessContext = (SetThreadDpiAwarenessContextFn*)GetProcAddress(User32, "SetThreadDpiAwarenessContext");
    GetThreadDpiAwarenessContextFn* GetThreadDpiAwarenessContext = (GetThreadDpiAwarenessContextFn*)GetProcAddress(User32, "GetThreadDpiAwarenessContext");
    GetWindowDpiAwarenessContextFn* GetWindowDpiAwarenessContext = (GetWindowDpiAwarenessContextFn*)GetProcAddress(User32, "GetWindowDpiAwarenessContext");
    GetAwarenessFromDpiAwarenessContextFn* GetAwarenessFromDpiAwarenessContext = (GetAwarenessFromDpiAwarenessContextFn*)GetProcAddress(User32, "GetAwarenessFromDpiAwarenessContext");
    GetDpiForWindowFn* GetDpiForWindow = (GetDpiForWindowFn*)GetProcAddress(User32, "GetDpiForWindow");
    EnableNonClientDpiScalingFn* EnableNonClientDpiScaling = (EnableNonClientDpiScalingFn*)GetProcAddress(User32, "EnableNonClientDpiScaling");

    AdjustWindowRectExForDpiFn* AdjustWindowRectExForDpi = (AdjustWindowRectExForDpiFn*)GetProcAddress(User32, "AdjustWindowRectExForDpi");

    // Detect if this platform has modern DPI support
    // As per Microsoft recommendation https://docs.microsoft.com/en-us/windows/desktop/SysInfo/operating-system-version, we don't try to check the OS version.
    bool IsAvailable() { return GetThreadDpiAwarenessContext != nullptr; }

    // The exact reasoning for checking EnableNonClientDpiScaling separately is lost to time.
    // I think it's becuase it's documented as having been introduced in Windows 10 1607 (which is newer than most of the other functions.)
    // (Although that doesn't seem right because it's meant to be used with per-monitor v1, which was introduced in 8.1)
    // Or maybe I saw this in some best practices in the DPI documentation back when I was reading through it all and forgot to link it.
    // Either way, it's cheap to check so we might as well.
    bool IsEnableNonClientDpiScalingAvailable() { return EnableNonClientDpiScaling != nullptr; }
}
