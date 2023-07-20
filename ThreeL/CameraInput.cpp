#include "pch.h"
#include "CameraInput.h"

#include <hidusage.h>
#include <imgui.h>
#include <xinput.h>
#include <windowsx.h>

// https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input#scan-codes
// (Use the "Scan 1 Make" column in the table -- image is misleading since it's 1-indexed)
namespace ScanCode
{
    enum Enum
    {
        W = 0x11,
        A = 0x1E,
        S = 0x1F,
        D = 0x20,
    };
}

CameraInput::CameraInput(Window& window)
    : m_Window(window)
{
    RefreshWasdName();

    // Enable raw mouse input
    RAWINPUTDEVICE rawMouse =
    {
        .usUsagePage = HID_USAGE_PAGE_GENERIC,
        .usUsage = HID_USAGE_GENERIC_MOUSE,
        .dwFlags = 0,
        .hwndTarget = window.Hwnd(),
    };
    AssertWinError(RegisterRawInputDevices(&rawMouse, 1, sizeof(rawMouse)));

    m_WndProcHandle = m_Window.AddWndProc([&](HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) -> std::optional<LRESULT>
        {
            switch (message)
            {
                case WM_KEYDOWN:
                case WM_KEYUP:
                {
                    uint32_t scanCode = LOBYTE(HIWORD(lParam));
                    bool* moveVar = nullptr;

                    if (scanCode == ScanCode::W || wParam == VK_UP) { moveVar = &m_MoveW; }
                    else if (scanCode == ScanCode::A || wParam == VK_LEFT) { moveVar = &m_MoveA; }
                    else if (scanCode == ScanCode::S || wParam == VK_DOWN) { moveVar = &m_MoveS; }
                    else if (scanCode == ScanCode::D || wParam == VK_RIGHT) { moveVar = &m_MoveD; }
                    else if (wParam == VK_SHIFT) { moveVar = &m_MoveFast; }
                    else if (wParam == VK_CONTROL) { moveVar = &m_MoveSlow; }

                    if (moveVar != nullptr)
                    { *moveVar = message == WM_KEYDOWN; }
                    break;
                }
                case WM_INPUT:
                {
                    RAWINPUT rawInput;
                    UINT size = sizeof(rawInput);
                    if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &rawInput, &size, sizeof(rawInput.header)) != -1
                        && rawInput.header.dwType == RIM_TYPEMOUSE
                        // We don't try to handle absolute input devices for camera input right now
                        && (rawInput.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0
                        )
                    {
                        m_NextMouseDelta.x += rawInput.data.mouse.lLastX;
                        m_NextMouseDelta.y += rawInput.data.mouse.lLastY;
                    }
                    return 0;
                }
                case WM_LBUTTONDOWN:
                {
                    // Don't enable mouselook when Dear ImGui wants the mouse or we somehow already have it
                    if (ImGui::GetIO().WantCaptureMouse || m_EnableLook)
                    { break; }

                    m_EnableLook = true;

                    // Capture and hide the cursor
                    SetCapture(hwnd);
                    ShowCursor(false);

                    POINT cursorPos;
                    GetCursorPos(&cursorPos);

                    RECT jail =
                    {
                        .left = cursorPos.x,
                        .top = cursorPos.y,
                        .right = cursorPos.x,
                        .bottom = cursorPos.y
                    };
                    ClipCursor(&jail);
                    break;
                }
                case WM_KILLFOCUS:
                    m_MoveW = m_MoveA = m_MoveS = m_MoveD = false;
                    [[fallthrough]];
                case WM_LBUTTONUP:
                    if (m_EnableLook)
                    {
                        m_EnableLook = false;

                        // Release the cursor
                        ClipCursor(nullptr);
                        ShowCursor(true);
                        ReleaseCapture();
                    }
                    break;
                case WM_INPUTLANGCHANGE:
                    RefreshWasdName();
                    return 1;
            }

            return { };
        });
}

void CameraInput::StartFrame(float deltaTime)
{
    m_MouseDelta = m_NextMouseDelta;
    m_NextMouseDelta = int2::Zero;

    m_MoveVector = float2::Zero;
    m_LookVector = float2::Zero;

    // Keyboard movement
    if (!ImGui::GetIO().WantCaptureKeyboard)
    {
        if (m_MoveW) { m_MoveVector.y += 1.f; }
        if (m_MoveS) { m_MoveVector.y -= 1.f; }
        if (m_MoveA) { m_MoveVector.x -= 1.f; }
        if (m_MoveD) { m_MoveVector.x += 1.f; }
    }

    if (m_MoveVector.LengthSquared() > 0.1f)
    {
        m_MoveVector = m_MoveVector.Normalized() * deltaTime * m_MoveSpeed;

        if (m_MoveSlow)
        { m_MoveVector = m_MoveVector * m_MoveSlowMultiplier; }
        else if (m_MoveFast)
        { m_MoveVector = m_MoveVector * m_MoveFastMultiplier; }
    }

    // Mouse movement
    if (m_EnableLook)
    {
        m_LookVector = ((float2)m_MouseDelta) * 0.01f * m_MouseSensitivity;
        m_LookVector.y *= -1.f; // Flip Y so positive is looking up
    }

    // XInput controller movement
    // (Applied last since it overrides the keyboard/mouse movement above)
    XINPUT_STATE xInputState;
    if (XInputGetState(0, &xInputState) == ERROR_SUCCESS)
    {
        XINPUT_GAMEPAD gamepad = xInputState.Gamepad;

        float2 extent = float2(32767.f);
        float2 leftStick = float2(gamepad.sThumbLX, gamepad.sThumbLY);
        float2 rightStick = float2(gamepad.sThumbRX, gamepad.sThumbRY);

        // "Debugging" this code is how I found out my dev Xbox controller had succumbed the drift virus :(
        if (leftStick.Length() > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
        { m_MoveVector = (leftStick / extent) * deltaTime * m_MoveSpeed; }

        if ((gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0)
        { m_MoveVector = m_MoveVector * m_MoveFastMultiplier; }

        if (rightStick.Length() > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
        { m_LookVector = (rightStick / extent) * deltaTime * m_ControllerLookSensitivity; }
    }
}

void CameraInput::RefreshWasdName()
{
    // Get the name of the "WASD" keys for the user's keyboard layout
    char buffer[512];
    char* bufferPtr = buffer;
    int bufferSize = (int)std::size(buffer);

    auto AppendKey = [&](ScanCode::Enum scanCode) -> bool
        {
            int length = GetKeyNameTextA(scanCode << 16, bufferPtr, bufferSize);
            bufferPtr += length;
            bufferSize -= length;
            return length > 0;
        };

    if (AppendKey(ScanCode::W) && AppendKey(ScanCode::A) && AppendKey(ScanCode::S) && AppendKey(ScanCode::D))
    {
        m_WasdName = std::string(buffer, bufferPtr - buffer);
    }
    else
    {
        // If something fails just fall back to displaying WASD
        m_WasdName = "WASD";
    }
}

CameraInput::~CameraInput()
{
    m_Window.RemoveWndProc(m_WndProcHandle);

    // Unsubscribe from raw mouse input messages
    RAWINPUTDEVICE rawMouse =
    {
        .usUsagePage = HID_USAGE_PAGE_GENERIC,
        .usUsage = HID_USAGE_GENERIC_MOUSE,
        .dwFlags = RIDEV_REMOVE,
        .hwndTarget = NULL,
    };
    AssertWinError(RegisterRawInputDevices(&rawMouse, 1, sizeof(rawMouse)));
}
