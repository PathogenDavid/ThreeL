#pragma once
#include "pch.h"

#include "Math.h"
#include "Window.h"

class CameraInput
{
private:
    Window& m_Window;
    WndProcHandle m_WndProcHandle;

    bool m_MoveW = false;
    bool m_MoveA = false;
    bool m_MoveS = false;
    bool m_MoveD = false;
    bool m_MoveSlow = false;
    bool m_MoveFast = false;

    bool m_EnableLook = false;
    int2 m_NextMouseDelta = int2::Zero;
    int2 m_MouseDelta = int2::Zero;

    std::string m_WasdName = "WASD";

    float2 m_MoveVector = float2::Zero;
    float2 m_LookVector = float2::Zero;

    float m_MoveSpeed = 3.f; // units per second
    float m_MoveFastMultiplier = 3.f;
    float m_MoveSlowMultiplier = 0.25f;

public:
    float m_LookSpeed = 0.01f;
    float m_MouseSensitivity = 1.f;
    float m_ControllerLookSensitivity = 3.f;

    CameraInput(Window& window);
    ~CameraInput();

    void StartFrame(float deltaTime);

    inline const std::string& WasdName() const { return m_WasdName; }

    inline float2 MoveVector() const { return m_MoveVector; }
    inline float2 LookVector() const { return m_LookVector; }

private:
    void RefreshWasdName();
};
