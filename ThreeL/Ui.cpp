#include "pch.h"
#include "Ui.h"

#include "CameraController.h"
#include "CameraInput.h"
#include "ComputeContext.h"
#include "DearImGui.h"
#include "DebugLayer.h"
#include "FrameStatistics.h"
#include "LightLinkedList.h"
#include "ParticleSystem.h"
#include "ParticleSystemDefinition.h"

#include <imgui_internal.h>

Ui::Ui(GraphicsCore& graphics, Window& window, DearImGui& dearImGui, CameraController& camera, CameraInput& cameraInput, FrameStatistics& stats)
    : m_Graphics(graphics), m_Window(window), m_DearImGui(dearImGui), m_Camera(camera), m_CameraInput(cameraInput), m_Stats(stats)
{
    m_SystemInfo = std::format("{}{}", std::wstring(m_Graphics.AdapterName()), DebugLayer::GetExtraWindowTitleInfo());
}

void Ui::Start(const uint2& screenSize, const float2& screenSizeF, const float4x4& perspectiveTransform)
{
    m_ScreenSize = screenSize;
    m_ScreenSizeF = screenSizeF;
    m_PerspectiveTransform = perspectiveTransform;
}

void Ui::BeginMainViewportWindow()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2());
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
    flags |= ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;
    ImGui::Begin("MainDockSpaceViewportWindow", nullptr, flags);
    ImGui::PopStyleVar(3);

    ImGui::DockSpace(0xC0FFEEEE, ImVec2(), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoDockingInCentralNode);
    m_CentralNode = ImGui::DockBuilderGetCentralNode(0xC0FFEEEE);
}

void Ui::SubmitFrameTimeIndicator(uint64_t frameNumber)
{
    ImGuiStyle& style = ImGui::GetStyle();
    float graphWidth = 200.f + style.FramePadding.x * 2.f;
    float frameTimeWidth = ImGui::CalcTextSize("GPU frame time: 9999.99 ms").x; // Use a static string so the position is stable

    float gpuFrameTime = (float)(m_Stats.GetElapsedTimeGpu(Timer::FrameTotal) * 1000.0);
    m_FrameTimeHistory[frameNumber % std::size(m_FrameTimeHistory)] = gpuFrameTime;

    float maxFrameTime = 0.f;
    for (uint32_t i = 0; i < std::size(m_FrameTimeHistory); i++)
    { maxFrameTime = std::max(m_FrameTimeHistory[i], maxFrameTime); }

    ImGui::SetCursorPosX(m_ScreenSizeF.x - graphWidth - frameTimeWidth);
    if (ImGui::SneakyButton(std::format("GPU frame time: {:0.2f} ms###frameTimeButton", gpuFrameTime).c_str()))
        ShowTimingStatisticsWindow = !ShowTimingStatisticsWindow;
    ImGui::SetCursorPosX(m_ScreenSizeF.x - graphWidth);
    ImGui::PlotLines
    (
        "##FrameTimeGraph",
        m_FrameTimeHistory,
        (int)std::size(m_FrameTimeHistory),
        (int)(frameNumber % std::size(m_FrameTimeHistory)),
        nullptr,
        0.f,
        // Avoid having a super noisy looking graph when frame times are low
        std::max(maxFrameTime, 1000.f / 60.f),
        ImVec2(200.f, ImGui::GetCurrentWindow()->MenuBarHeight())
    );
}

void Ui::SubmitLightLinkedListSettingsWindow(uint32_t& lightLinkedListShift, uint32_t& lightLinkLimit, uint32_t downsampledDepthBufferCount)
{
    ImGui::SetNextWindowSize(ImVec2(275.f * m_DearImGui.DpiScale(), 0.f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin2("Light linked list settings", &ShowLightLinkedListSettingsWindow))
    {
        ImGui::PushItemWidth(-FLT_MIN);
        char comboTemp[128];
        uint2 size = LightLinkedList::ScreenSizeToLllBufferSize(m_ScreenSize, lightLinkedListShift);
        uint2 currentLightBufferSize = size;
        snprintf(comboTemp, sizeof(comboTemp), "1/%d (%dx%d)", (int)std::pow(2, lightLinkedListShift), size.x, size.y);
        ImGui::TextUnformatted("Buffer size");
        if (ImGui::BeginCombo("##lightLinkedListShiftCombo", comboTemp))
        {
            for (uint32_t div = 1, i = 0; i <= downsampledDepthBufferCount; div *= 2, i++)
            {
                size = LightLinkedList::ScreenSizeToLllBufferSize(m_ScreenSize, i);
                snprintf(comboTemp, sizeof(comboTemp), "1/%d (%dx%d)", div, size.x, size.y);
                bool isSelected = lightLinkedListShift == i;
                if (ImGui::Selectable(comboTemp, isSelected))
                {
                    lightLinkedListShift = i;
                }

                if (isSelected) { ImGui::SetItemDefaultFocus(); }
            }
            ImGui::EndCombo();
        }

        {
            ImGui::Text("Light links heap capacity");
            double speed = 16.0 + std::pow((double)lightLinkLimit / (double)LightLinkedList::MAX_LIGHT_LINKS, 2.0) * 5120000.0;
            ImGui::DragInt("##lightLinksLimit", &lightLinkLimit, (float)std::max(1.0, speed), 0, LightLinkedList::MAX_LIGHT_LINKS, "%u", ImGuiSliderFlags_AlwaysClamp);
        }

        ImGui::SeparatorText("Buffer Sizes");
        {
            const char* sizeUnits;
            size_t sizeBytes;
            double size;
            size_t totalSize = 0;

            totalSize += sizeBytes = currentLightBufferSize.x * currentLightBufferSize.y * sizeof(uint32_t);
            size = GetHumanFriendlySize(sizeBytes, sizeUnits);
            ImGui::Text("First link buffer: %.2f %s", size, sizeUnits);

            totalSize += sizeBytes = lightLinkLimit * ShaderInterop::SizeOfLightLink;
            double heapUsed = ((double)m_Stats.NumberOfLightLinksUsed() / (double)lightLinkLimit) * 100.0;
            size = GetHumanFriendlySize(sizeBytes, sizeUnits);
            ImGui::Text(" Light links heap: %.2f %s (%.2f%%)", size, sizeUnits, heapUsed);

            size = GetHumanFriendlySize(sizeBytes, sizeUnits);
            ImGui::Text("            Total: %.2f %s", size, sizeUnits);
        }

        ImGui::SeparatorText("Frame Statistics");
        {
            ImGui::Text("Light links used: %d", m_Stats.NumberOfLightLinksUsed());
            ImGui::Text("Max lights per pixel: %d", m_Stats.MaximumLightCountForAnyPixel());
            ImGui::Text("Average lights per pixel: %.1f", (double)m_Stats.NumberOfLightLinksUsed() / (double)(currentLightBufferSize.x * currentLightBufferSize.y));
        }

        ImGui::PopItemWidth();
        ImGui::End();
    }
}

void Ui::SubmitParticleSystemEditor(ComputeContext& context, ParticleSystem& particleSystem, ParticleSystemDefinition& particleSystemDefinition)
{
    ImGui::SetNextWindowSize(ImVec2(275.f * m_DearImGui.DpiScale(), 0.f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin2("Particle Editor", &ShowParticleSystemEditor))
    {
        ImGui::PushItemWidth(-90.f);
        ImGui::DragFloat("Spawn rate", &particleSystemDefinition.SpawnRate, 0.1f, 0.f, FLT_MAX, "%.2f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::SliderFloat("Max size", &particleSystemDefinition.MaxSize, 0.f, 1.f, "%.3f", ImGuiSliderFlags_AlwaysClamp);

        ImGui::SeparatorText("Lifetime");
        ImGui::SliderFloat("Fade out time", &particleSystemDefinition.FadeOutTime, 0.f, particleSystemDefinition.LifeMin, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloatRange2("Lifetime", &particleSystemDefinition.LifeMin, &particleSystemDefinition.LifeMax, 0.1f, 0.f, FLT_MAX, "%.1f s", nullptr, ImGuiSliderFlags_AlwaysClamp);

        ImGui::SeparatorText("Movement");
        ImGui::Text("Velocity direction variance");
        ImGui::DragFloat("X##VelocityDirectionVariance", &particleSystemDefinition.VelocityDirectionVariance.x, 0.01f, 0.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Y##VelocityDirectionVariance", &particleSystemDefinition.VelocityDirectionVariance.y, 0.01f, 0.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Z##VelocityDirectionVariance", &particleSystemDefinition.VelocityDirectionVariance.z, 0.01f, 0.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::Text("Velocity direction bias");
        ImGui::DragFloat("X##VelocityDirectionBias", &particleSystemDefinition.VelocityDirectionBias.x, 0.01f, -FLT_MAX, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Y##VelocityDirectionBias", &particleSystemDefinition.VelocityDirectionBias.y, 0.01f, -FLT_MAX, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Z##VelocityDirectionBias", &particleSystemDefinition.VelocityDirectionBias.z, 0.01f, -FLT_MAX, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::Text("Velocity magnitude");
        ImGui::DragFloatRange2("Linear", &particleSystemDefinition.VelocityMagnitudeMin, &particleSystemDefinition.VelocityMagnitudeMax, 0.01f);
        ImGui::DragFloatRange2("Angular", &particleSystemDefinition.AngularVelocityMin, &particleSystemDefinition.AngularVelocityMax, 0.01f, 0.f, 0.f, "%.3f rad/s");

        ImGui::SeparatorText("Spawn point");
        ImGui::Checkbox("Show spawn point gizmo", &ShowParticleSystemGizmo);
        ImGui::Text("Spawn point variance");
        ImGui::DragFloat("X##SpawnPointVariance", &particleSystemDefinition.SpawnPointVariance.x, 0.01f, 0.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Y##SpawnPointVariance", &particleSystemDefinition.SpawnPointVariance.y, 0.01f, 0.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("Z##SpawnPointVariance", &particleSystemDefinition.SpawnPointVariance.z, 0.01f, 0.f, FLT_MAX, "%.3f", ImGuiSliderFlags_AlwaysClamp);

        ImGui::SeparatorText("Visuals");
        ImGui::ColorEdit3("Base color", &particleSystemDefinition.BaseColor.x);
        ImGui::DragFloatRange2("Shade", &particleSystemDefinition.MinShade, &particleSystemDefinition.MaxShade, 0.001f, 0.f, 1.f, "%.3f", nullptr, ImGuiSliderFlags_AlwaysClamp);

        ImGui::SeparatorText("Commands");
        if (ImGui::Button("Reset", ImVec2(-1.f, 0.f)))
            particleSystem.Reset(context);

        static float seedSeconds = 10.f;
        ImGui::PopItemWidth();
        ImGui::PushItemWidth(-FLT_MIN);
        if (ImGui::Button("Advance system by"))
            particleSystem.SeedState(seedSeconds);
        ImGui::SameLine();
        ImGui::SliderFloat("##seedSeconds", &seedSeconds, 1.f / 30.f, particleSystemDefinition.LifeMax, "%.1f seconds");
        ImGui::PopItemWidth();
        ImGui::End();

        if (ShowParticleSystemGizmo)
        {
            float3 spawnPoint = particleSystem.SpawnPoint();
            if (ImGuizmo::Manipulate(m_Camera.ViewTransform(), m_PerspectiveTransform, ImGuizmo::TRANSLATE, ImGuizmo::WORLD, spawnPoint))
            { particleSystem.SpawnPoint(spawnPoint); }
        }
    }
}

static void TimerRow(FrameStatistics& stats, float timeWidth, const char* name, Timer timer)
{
    double elapsedCpu = stats.GetElapsedTimeCpu(timer);
    double elapsedGpu = stats.GetElapsedTimeGpu(timer);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", name);

    ImGui::TableSetColumnIndex(1);
    if (elapsedGpu == FrameStatistics::TIMER_SKIPPED)
        ImGui::RightAlignedText("N/A", timeWidth);
    else if (elapsedGpu == FrameStatistics::TIMER_INVALID)
        ImGui::RightAlignedText("ERROR", timeWidth);
    else
        ImGui::RightAlignedText(std::format("{:0.2f}", elapsedGpu * 1000.0), timeWidth);

    ImGui::TableSetColumnIndex(2);
    if (elapsedCpu == FrameStatistics::TIMER_SKIPPED)
        ImGui::RightAlignedText("N/A", timeWidth);
    else if (elapsedCpu == FrameStatistics::TIMER_INVALID)
        ImGui::RightAlignedText("ERROR", timeWidth);
    else
        ImGui::RightAlignedText(std::format("{:0.2f}", elapsedCpu * 1000.0), timeWidth);
}

void Ui::SubmitTimingStatisticsWindow()
{
    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(m_CentralNode->Pos.x + m_CentralNode->Size.x, m_CentralNode->Pos.y), ImGuiCond_Always, ImVec2(1.f, 0.f));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMouseInputs;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.f, 0.f));
    if (ImGui::Begin2("Timing Statistics", &ShowTimingStatisticsWindow, flags))
    {
        ImGuiTableFlags tableFlags = 0;
        if (ImGui::BeginTable("TimingStats", 3, tableFlags))
        {
            float maxTimeWidth = ImGui::CalcTextSize("999.99").x;
            ImGui::TableSetupColumn("Measurement");
            ImGui::TableSetupColumn("GPU", ImGuiTableColumnFlags_WidthFixed, maxTimeWidth);
            ImGui::TableSetupColumn("CPU", ImGuiTableColumnFlags_WidthFixed, maxTimeWidth);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Measurement");
            ImGui::TableSetColumnIndex(1);
            ImGui::RightAlignedText("GPU", maxTimeWidth);
            ImGui::TableSetColumnIndex(2);
            ImGui::RightAlignedText("CPU", maxTimeWidth);

            TimerRow(m_Stats, maxTimeWidth, "FrameSetup", Timer::FrameSetup);
            TimerRow(m_Stats, maxTimeWidth, "ParticleUpdate", Timer::ParticleUpdate);
            TimerRow(m_Stats, maxTimeWidth, "DepthPrePass", Timer::DepthPrePass);
            TimerRow(m_Stats, maxTimeWidth, "DepthDownsample", Timer::DownsampleDepth);
            TimerRow(m_Stats, maxTimeWidth, "FillLightLinkedList", Timer::FillLightLinkedList);
            TimerRow(m_Stats, maxTimeWidth, "OpaquePass", Timer::OpaquePass);
            TimerRow(m_Stats, maxTimeWidth, "ParticleRender", Timer::ParticleRender);
            TimerRow(m_Stats, maxTimeWidth, "DebugOverlay", Timer::DebugOverlay);
            TimerRow(m_Stats, maxTimeWidth, "UI", Timer::UI);
            TimerRow(m_Stats, maxTimeWidth, "Present", Timer::Present);
            TimerRow(m_Stats, maxTimeWidth, "FrameTotal", Timer::FrameTotal);

            ImGui::EndTable();
        }
        ImGui::End();
    }
    ImGui::PopStyleVar();
}

void Ui::SubmitViewportOverlays(ShaderInterop::LightLinkedListDebugMode debugOverlay, uint32_t maxLightsPerPixelForOverlay)
{
    using ShaderInterop::LightLinkedListDebugMode;
    float padding = 5.f;
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    ImU32 white = IM_COL32(255, 255, 255, 255);
    ImU32 black = IM_COL32(0, 0, 0, 255);

    // Show overlay legend
    float overlayLegendWidth = 0.f;
    if (debugOverlay == LightLinkedListDebugMode::LightCount)
    {
        float w = std::min(m_ScreenSizeF.x * 0.25f, 270.f * m_DearImGui.DpiScale());
        overlayLegendWidth = w + padding + 2.f;
        float h = 20.f * m_DearImGui.DpiScale();

        float x0 = m_CentralNode->Pos.x + m_CentralNode->Size.x - w - padding - 1.f;
        float x1 = x0 + (w / 3.f);
        float x2 = x0 + (w / 3.f * 2.f);
        float x3 = x0 + w;

        float y0 = m_CentralNode->Pos.y + m_CentralNode->Size.y - h - padding - 1.f;
        float y1 = y0 + h;

        drawList->AddRectFilled(ImVec2(x0 - 1.f, y0 - 1.f), ImVec2(x3 + 1.f, y1 + 1.f), white);
        uint32_t color0 = IM_COL32(0, 0, 255, 255);
        uint32_t color1 = IM_COL32(0, 255, 255, 255);
        uint32_t color2 = IM_COL32(0, 255, 0, 255);
        uint32_t color3 = IM_COL32(255, 255, 0, 255);
        drawList->AddRectFilledMultiColor(ImVec2(x0, y0), ImVec2(x1, y1), color0, color1, color1, color0);
        drawList->AddRectFilledMultiColor(ImVec2(x1, y0), ImVec2(x2, y1), color1, color2, color2, color1);
        drawList->AddRectFilledMultiColor(ImVec2(x2, y0), ImVec2(x3, y1), color2, color3, color3, color2);

        std::string labelMax = std::format("{}", maxLightsPerPixelForOverlay);
        ImVec2 labelSize = ImGui::CalcTextSize(labelMax);
        float labelY = y0 + h * 0.5f - labelSize.y * 0.5f;
        ImGui::HudText(drawList, ImVec2(x0 + padding, labelY), "0");
        ImGui::HudText(drawList, ImVec2(x3 - labelSize.x - padding, labelY), labelMax);
    }

    // Show controls hint
    if (ShowControlsHint)
    {
        std::string controlsHint = std::format("Move with {}, click+drag look around or use Xbox controller.", m_CameraInput.WasdName());
        float wrapWidth = m_CentralNode->Size.x - padding * 2.f - overlayLegendWidth;
        ImVec2 hintSize = ImGui::CalcTextSize(controlsHint, false, wrapWidth);
        ImVec2 position = ImVec2(m_CentralNode->Pos.x + padding, m_CentralNode->Pos.y + m_CentralNode->Size.y - hintSize.y - padding);
        ImGui::HudText(drawList, position, controlsHint, wrapWidth);
    }

    // Show runtime info in fullscreen
    if (m_Window.CurrentOutputMode() != OutputMode::Windowed)
    {
        ImVec2 position = ImVec2(m_CentralNode->Pos.x + padding, m_CentralNode->Pos.y + padding);
        ImGui::HudText(drawList, position, m_SystemInfo);
    }
}
