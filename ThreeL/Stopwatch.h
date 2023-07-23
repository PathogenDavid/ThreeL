#pragma once
#include <Windows.h>

class Stopwatch
{
private:
    static double s_Frequency;
    LARGE_INTEGER m_Start;

public:
    Stopwatch()
    {
        Restart();
    }

    inline void Restart() { QueryPerformanceCounter(&m_Start); }

    inline double ElapsedSeconds() const
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return double(now.QuadPart - m_Start.QuadPart) / s_Frequency;
    }
};
