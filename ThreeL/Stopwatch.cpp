#include "pch.h"
#include "Stopwatch.h"

static double GetFrequency()
{
    LARGE_INTEGER frequency;
    Assert(QueryPerformanceFrequency(&frequency));
    return (double)frequency.QuadPart;
}

double Stopwatch::s_Frequency = GetFrequency();
