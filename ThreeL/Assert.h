#pragma once
#include <Windows.h>

enum class AssertKind
{
    Normal,
    WinError,
    HResult,
};

void HandleFailedAssert(const char* condition, const char* fileName, int lineNumber, AssertKind kind, HRESULT errorCode);

#ifdef NDEBUG
// Unlike the standard assert macro we still evaluate the condition to make HRESULT checking easier and others consitent with it
// For conditions without side effect we can expect the optimizer to eliminate them
#define _AssertRaw(cond, kind, errorCode) ((void)cond)
#else
#define _AssertRaw(cond, kind, errorCode) (void)((!!(cond)) || (HandleFailedAssert( #cond, __FILE__, __LINE__, (kind), (errorCode)), 0))
#endif

#define Assert(cond) _AssertRaw(cond, AssertKind::Normal, 0)
#define AssertWinError(cond) _AssertRaw(cond, AssertKind::WinError, (HRESULT)GetLastError())
#define AssertSuccess(hresult) _AssertRaw(SUCCEEDED(hresult), AssertKind::HResult, hresult)

void Fail(const char* message);
