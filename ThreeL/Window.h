#pragma once
#include <functional>
#include <optional>
#include <vector>
#include <Windows.h>

using WndProcFunction = std::function<std::optional<LRESULT>(HWND, UINT, WPARAM, LPARAM)>;

struct WndProcHandle
{
    friend class Window;
private:
    size_t m_Handle;

    WndProcHandle(size_t handle)
        : m_Handle(handle)
    {
    }

public:
    WndProcHandle()
        : m_Handle(0)
    {
    }
};

class Window
{
private:
    HINSTANCE m_ThisProcessModuleHandle;
    ATOM m_WindowClass;
    HWND m_Hwnd;

    size_t m_LastMessageHandlerId = 0;
    std::vector<std::pair<size_t, WndProcFunction>> m_MessageHandlers;

public:
    Window(LPCWSTR title, int width, int height);
    ~Window();

    void Show();

    //! Returns true if app should continue running
    static bool ProcessMessages();

    inline HWND GetHwnd()
    {
        return m_Hwnd;
    }

    WndProcHandle AddWndProc(WndProcFunction function);
    void RemoveWndProc(WndProcHandle handle);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(UINT message, WPARAM wParam, LPARAM lParam);
};
