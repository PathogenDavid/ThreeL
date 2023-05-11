#include "Assert.h"

#include <format>
#include <stdio.h>
#include <Windows.h>

static void PrintMessageForHResultOrWinError(HRESULT errorCode)
{
    LPSTR message;
    DWORD length = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        (LPSTR)&message,
        0,
        nullptr
    );

    if (length == 0 || message == nullptr)
        fprintf(stderr, "<error %d while fetching message>\n", GetLastError());
    else
        fprintf(stderr, "%s", message);

    if (message != nullptr)
        LocalFree((HLOCAL)message);
}

static void PrintFailedAssert(const char* condition, const char* fileName, int lineNumber, AssertKind kind, HRESULT errorCode)
{
    fprintf(stderr, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

    if (kind != AssertKind::HResult)
    {
        fprintf(stderr, "Assertion");
        if (strcmp(condition, "false") != 0)
        {
            fprintf(stderr, " '%s'", condition);
        }
        fprintf(stderr, " failed at %s:%d\n", fileName, lineNumber);

        switch (kind)
        {
            case AssertKind::WinError:
            {
                fprintf(stderr, "GetLastError %d: ", errorCode);
                PrintMessageForHResultOrWinError(errorCode);
                break;
            }
        }
    }
    else
    {
        fprintf(stderr, "Failed HRESULT 0x%8x: ", errorCode);
        PrintMessageForHResultOrWinError(errorCode);
    }

    fprintf(stderr, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
}

void HandleFailedAssert(const char* condition, const char* fileName, int lineNumber, AssertKind kind, HRESULT errorCode)
{
    PrintFailedAssert(condition, fileName, lineNumber, kind, errorCode);
    DebugBreak();
    abort();
}

void Fail(const char* message)
{
#ifdef DEBUG
    fprintf(stderr, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    fprintf(stderr, "Fatal error: %s\n", message);
    fprintf(stderr, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    DebugBreak();
#else
    MessageBoxA(nullptr, message, "Fatal Error", MB_ICONERROR);
#endif
    abort();
}
