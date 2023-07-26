#pragma once
#include "Matrix4.h"
#include "ShaderInterop.h"
#include "Vector2.h"

class CameraController;
class CameraInput;
struct ComputeContext;
class DearImGui;
class FrameStatistics;
class GraphicsCore;
struct ImGuiDockNode;
class ParticleSystem;
struct ParticleSystemDefinition;
class Window;

class Ui
{
private:
    GraphicsCore& m_Graphics;
    Window& m_Window;
    DearImGui& m_DearImGui;
    CameraController& m_Camera;
    CameraInput& m_CameraInput;
    FrameStatistics& m_Stats;

    std::string m_SystemInfo;

    ImGuiDockNode* m_CentralNode = nullptr;
    float4x4 m_PerspectiveTransform;
    uint2 m_ScreenSize;
    float2 m_ScreenSizeF;
public:
    Ui(GraphicsCore& graphics, Window& window, DearImGui& dearImGui, CameraController& camera, CameraInput& cameraInput, FrameStatistics& stats);

    void Start(const uint2& screenSize, const float2& screenSizeF, const float4x4& perspectiveTransform);

    void BeginMainViewportWindow();

private:
    float m_FrameTimeHistory[200] = { };
public:
    void SubmitFrameTimeIndicator(uint64_t frameNumber);

    bool ShowLightLinkedListSettingsWindow = true;
    void SubmitLightLinkedListSettingsWindow(uint32_t& lightLinkedListShift, uint32_t& lightLinkLimit, uint32_t downsampledDepthBufferCount);

    bool ShowParticleSystemEditor = false;
    bool ShowParticleSystemGizmo = true;
    void SubmitParticleSystemEditor(ComputeContext& context, ParticleSystem& particleSystem, ParticleSystemDefinition& particleSystemDefinition);

    bool ShowTimingStatisticsWindow = false;
    void SubmitTimingStatisticsWindow();

    bool ShowControlsHint = true;
    void SubmitViewportOverlays(ShaderInterop::LightLinkedListDebugMode debugOverlay, uint32_t maxLightsPerPixelForOverlay);

    inline ImGuiDockNode* CentralNode() const { return m_CentralNode; }
};
