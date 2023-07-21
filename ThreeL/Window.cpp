#include "pch.h"
#include "Window.h"

#include "Math.h"
#include "ModernDpi.h"

#include <ranges>
#include <ShellScalingApi.h>

struct Monitor
{
    HMONITOR Monitor;
    MONITORINFO Info;
};

static Monitor GetPrimaryMonitor()
{
    Monitor monitor = { };

    EnumDisplayMonitors(NULL, NULL, [](HMONITOR monitor, HDC, LPRECT, LPARAM extra) -> BOOL
        {
            MONITORINFO monitorInfo = { sizeof(monitorInfo) };
            if (GetMonitorInfoW(monitor, &monitorInfo) && (monitorInfo.dwFlags & MONITORINFOF_PRIMARY) != 0)
            {
                Monitor* result = (Monitor*)extra;
                result->Monitor = monitor;
                result->Info = monitorInfo;
                return FALSE;
            }

            return TRUE;
        }, (LPARAM)&monitor);

    return monitor;
}

Window::Window(LPCWSTR title, int width, int height)
{
    m_ThisProcessModuleHandle = GetModuleHandleW(nullptr);
    Assert(m_ThisProcessModuleHandle != nullptr);

    // Register the window class
    {
        WNDCLASSEXW wc =
        {
            .cbSize = sizeof(WNDCLASSEXW),
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = Window::WndProc,
            .hInstance = m_ThisProcessModuleHandle,
            .hCursor = LoadCursor(nullptr, IDC_ARROW),
            
            // It is important that the background color is null here.
            // If we don't, Windows will "pre-paint" us when we are resized, which creates an annoying flicker effect.
            // This happens even though we ignore WM_ERASEBKGND.
            // This is similar to the solution Winforms uses:
            // https://github.com/dotnet/winforms/blob/3ce29ac28e6cf25dbb3538543c98376f1d7a0da9/src/System.Windows.Forms/src/System/Windows/Forms/NativeWindow.cs#L1521-L1523
            // The distinction between using HOLLOW_BRUSH and null does not seem to actually matter. The Old New Thing blog linked below seems to imply
            // that with null you won't get WM_ERASEBKGND, but that does not actually seem to be the case. (I get it either way.)
            // Looking with WinSpy++, Unity and SDL use null too.
            // https://devblogs.microsoft.com/oldnewthing/?p=1593
            .hbrBackground = nullptr,

            .lpszMenuName = nullptr,
            .lpszClassName = L"AresWindowClass",
        };

        m_WindowClass = RegisterClassExW(&wc);
        AssertWinError(m_WindowClass != 0);
    }

    // Create the main window
    {
        DWORD styleEx = 0;
        DWORD style = WS_OVERLAPPEDWINDOW;

        Monitor monitor = GetPrimaryMonitor();

        // Apply target monitor's DPI scale to the initial window size
        UINT dpi = USER_DEFAULT_SCREEN_DPI;
        if (monitor.Monitor != NULL && ModernDpi::IsAvailable())
        {
            UINT dpiX, dpiY;
            HRESULT hr = GetDpiForMonitor(monitor.Monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
            AssertSuccess(hr);
            if (SUCCEEDED(hr))
            {
                Assert(dpiX == dpiY && "The X and Y DPIs should match!");
                dpi = dpiX;
                width = MulDiv(width, dpiX, USER_DEFAULT_SCREEN_DPI);
                height = MulDiv(height, dpiY, USER_DEFAULT_SCREEN_DPI);
            }
        }

        RECT rect = { 0, 0, width, height };

        if (ModernDpi::IsAvailable())
        { Assert(ModernDpi::AdjustWindowRectExForDpi(&rect, style, false, styleEx, dpi)); }
        else
        { Assert(AdjustWindowRectEx(&rect, style, false, styleEx)); }

        int2 size(rect.right - rect.left, rect.bottom - rect.top);
        int2 position = int2(CW_USEDEFAULT, CW_USEDEFAULT);

        if (monitor.Monitor != NULL)
        {
            position.x = monitor.Info.rcWork.left + (monitor.Info.rcWork.right - monitor.Info.rcWork.left) / 2 - (size.x / 2);
            position.y = monitor.Info.rcWork.top + (monitor.Info.rcWork.bottom - monitor.Info.rcWork.top) / 2 - (size.y / 2);
        }

        m_Hwnd = nullptr;
        m_Hwnd = CreateWindowExW
        (
            styleEx,
            (LPCWSTR)m_WindowClass,
            title,
            style,
            position.x,
            position.y,
            size.x,
            size.y,
            nullptr,
            nullptr,
            m_ThisProcessModuleHandle,
            this
        );
        AssertWinError(m_Hwnd != nullptr);
    }
}

void Window::Show()
{
    ShowWindow(m_Hwnd, SW_SHOW);
}

bool Window::ProcessMessages()
{
    MSG message = {};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);

        if (message.message == WM_QUIT)
            return false;
    }

    return true;
}

WndProcHandle Window::AddWndProc(WndProcFunction function)
{
    m_LastMessageHandlerId++;
    size_t newHandle = m_LastMessageHandlerId;
    m_MessageHandlers.push_back({ newHandle, function });
    return { newHandle };
}

void Window::RemoveWndProc(WndProcHandle handle)
{
    for (auto it = m_MessageHandlers.begin(); it != m_MessageHandlers.end(); it++)
    {
        if (it->first == handle.m_Handle)
        {
            m_MessageHandlers.erase(it);
            return;
        }
    }

    Assert(false && "Window's message handler chain does not contain the specified handle.");
}

LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Handle window creation
    if (message == WM_NCCREATE)
    {
        Assert(GetWindowLongPtrW(hwnd, GWLP_USERDATA) == 0); // WM_NCCREATE should not be sent to windows more than once.

        // Enable non-client area DPI scaling for legacy platforms
        // This isn't necessary on platforms that support Per-Monitor DPI awareness v2, but you can't easily check for that so we just do it regardless.
        if (ModernDpi::IsEnableNonClientDpiScalingAvailable())
        { ModernDpi::EnableNonClientDpiScaling(hwnd); }

        // Save the Window's pointer in the window's user data
        CREATESTRUCTW* windowCreationArguments = (CREATESTRUCTW*)lParam;
        Assert(windowCreationArguments->lpCreateParams != nullptr);

        SetLastError(ERROR_SUCCESS); // SetWindowLongPtr does not clear the last error
        LONG_PTR oldUserData = SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)windowCreationArguments->lpCreateParams);
        Assert(oldUserData == 0);
        Assert(GetLastError() == ERROR_SUCCESS);

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    // Get the Window's pointer from the window's user data
    Window* window = (Window*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    // Dispatch the window procedure (or use the default one if the window isn't fully created yet)
    if (window == nullptr || window->m_Hwnd == nullptr)
    {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
    else
    {
        Assert(window->m_Hwnd == hwnd);
        return window->WndProc(message, wParam, lParam);
    }
}

LRESULT Window::WndProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    // Process registered message handlers (from most recently added to last.)
    for (auto messageHandler : m_MessageHandlers | std::views::reverse)
    {
        std::optional result = messageHandler.second(m_Hwnd, message, wParam, lParam);

        if (result.has_value())
            return result.value();
    }

    // No custom handlers handled the message, check if we have our own default behavior for this message
    switch (message)
    {
        //TODO: Handle WM_DPICHANGED
        case WM_PAINT:
        {
            // Explicitly tell Windows we don't need to paint since we don't need Win32 painting.
            // We can't just return without doing anything, Windows will keep sending WM_PAINT until we do.
            // We also don't want to let DefWindowProc take it since it has unspecified magical backwards-compatibility concerns.
            // https://devblogs.microsoft.com/oldnewthing/20141203-00/?p=43483
            Assert(ValidateRect(m_Hwnd, nullptr));
            return 0;
        }
        case WM_ERASEBKGND:
        {
            // We have nothing to do here, just do nothing and tell Windows we handled it.
            return 1;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
        default:
        {
            return DefWindowProcW(m_Hwnd, message, wParam, lParam);
        }
    }
}

Window::~Window()
{
    if (m_WindowClass != 0)
    {
        Assert(UnregisterClassW((LPCWSTR)m_WindowClass, m_ThisProcessModuleHandle));
        m_WindowClass = 0;
    }
}
