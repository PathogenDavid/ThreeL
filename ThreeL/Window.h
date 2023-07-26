#pragma once
#include "pch.h"

#include <functional>

using WndProcFunction = std::function<std::optional<LRESULT>(HWND, UINT, WPARAM, LPARAM)>;

struct WndProcHandle
{
    friend class Window;
private:
    size_t m_Handle;

    WndProcHandle(size_t handle)
        : m_Handle(handle)
    { }

public:
    WndProcHandle()
        : m_Handle(0)
    { }
};

enum class OutputMode
{
    Windowed,
    Fullscreen,
    MostlyFullscreen,
};

class Window
{
private:
    HINSTANCE m_ThisProcessModuleHandle;
    ATOM m_WindowClass;
    HWND m_Hwnd;

    size_t m_LastMessageHandlerId = 0;
    std::vector<std::pair<size_t, WndProcFunction>> m_MessageHandlers;

    OutputMode m_CurrentOutputMode = OutputMode::Windowed;
    OutputMode m_DesiredOutputMode = OutputMode::Windowed;
    RECT m_LastWindowedRect;
    bool m_LastWindowedMaximized = false;
    bool m_SuppressFullscreenWindowChanges = true;

public:
    Window(LPCWSTR title, int width, int height);
    ~Window();

    void Show();

    void StartFrame();

    //! Returns true if app should continue running
    static bool ProcessMessages();

    inline HWND Hwnd() const { return m_Hwnd; }
    inline OutputMode CurrentOutputMode() const { return m_CurrentOutputMode; }

    WndProcHandle AddWndProc(WndProcFunction function);
    void RemoveWndProc(WndProcHandle handle);

private:
    void ChangeOutputMode(OutputMode desiredMode);
public:
    void RequestOutputMode(OutputMode desiredMode) { m_DesiredOutputMode = desiredMode; }
    void CycleOutputMode();

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(UINT message, WPARAM wParam, LPARAM lParam);
};
