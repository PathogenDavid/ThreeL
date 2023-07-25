#include "pch.h"
#include "Ui.h"

#include "DearImGui.h"
#include "FrameStatistics.h"

#include <imgui_internal.h>

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

static void ShowTimer(FrameStatistics& stats, float timeWidth, const char* name, Timer timer)
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

void Ui::SubmitTimingStatisticsWindow(FrameStatistics& stats)
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

            ShowTimer(stats, maxTimeWidth, "FrameSetup", Timer::FrameSetup);
            ShowTimer(stats, maxTimeWidth, "ParticleUpdate", Timer::ParticleUpdate);
            ShowTimer(stats, maxTimeWidth, "DepthPrePass", Timer::DepthPrePass);
            ShowTimer(stats, maxTimeWidth, "DepthDownsample", Timer::DownsampleDepth);
            ShowTimer(stats, maxTimeWidth, "FillLightLinkedList", Timer::FillLightLinkedList);
            ShowTimer(stats, maxTimeWidth, "OpaquePass", Timer::OpaquePass);
            ShowTimer(stats, maxTimeWidth, "ParticleRender", Timer::ParticleRender);
            ShowTimer(stats, maxTimeWidth, "DebugOverlay", Timer::DebugOverlay);
            ShowTimer(stats, maxTimeWidth, "UI", Timer::UI);
            ShowTimer(stats, maxTimeWidth, "Present", Timer::Present);
            ShowTimer(stats, maxTimeWidth, "FrameTotal", Timer::FrameTotal);

            ImGui::EndTable();
        }
        ImGui::End();
    }
    ImGui::PopStyleVar();
}
