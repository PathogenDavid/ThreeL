#pragma once

class FrameStatistics;
struct ImGuiDockNode;

class Ui
{
private:
    ImGuiDockNode* m_CentralNode = nullptr;
public:
    Ui() = default;
    void BeginMainViewportWindow();

    bool ShowTimingStatisticsWindow = false;
    void SubmitTimingStatisticsWindow(FrameStatistics& stats);

    inline ImGuiDockNode* CentralNode() const { return m_CentralNode; }
};
