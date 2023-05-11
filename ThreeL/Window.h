#pragma once
#include <Windows.h>

class Window
{
private:
    HINSTANCE m_ThisProcessModuleHandle;
    ATOM m_WindowClass;
    HWND m_Hwnd;

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

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(UINT message, WPARAM wParam, LPARAM lParam);
};
