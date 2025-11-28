#include "Platform.h"

#include <cstdarg>
#include <cstdio>
#include <thread>

#include "Core/Window.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

int Platform::Snprintf(char* buffer, size_t size, const char* format, ...)
{
    va_list args;
    va_start(args, format);

#ifdef _WIN32
    int result = vsprintf_s(buffer, size, format, args);
#else
    int result = vsnprintf(buffer, size, format, args);
#endif

    va_end(args);
    return result;
}

Platform::ErrorDialogResult Platform::CreateErrorDialog(const char* title, const char* message, DialogOption option)
{
#ifdef _WIN32
    unsigned char messageBoxType = MB_OKCANCEL | MB_ICONERROR;
    switch (option)
    {
        case DialogOption::OkCancel:
            messageBoxType = MB_OKCANCEL;
            break;
        case DialogOption::YesNo:
            messageBoxType = MB_YESNO;
            break;
        case DialogOption::AbortRetryIgnore:
            messageBoxType = MB_ABORTRETRYIGNORE;
            break;
        case DialogOption::Ok:
            messageBoxType = MB_OK;
            break;
        case DialogOption::RetryCancel:
            messageBoxType = MB_RETRYCANCEL;
            break;
        case DialogOption::YesNoCancel:
            messageBoxType = MB_YESNOCANCEL;
            break;
    }
    int result = MessageBox(nullptr, message, title, messageBoxType | MB_ICONERROR);
    switch (result)
    {
        case IDOK:
            return ErrorDialogResult::Ok;
        case IDCANCEL:
            return ErrorDialogResult::Cancel;
        case IDABORT:
            return ErrorDialogResult::Abort;
        case IDRETRY:
            return ErrorDialogResult::Retry;
        case IDIGNORE:
            return ErrorDialogResult::Ignore;
        case IDYES:
            return ErrorDialogResult::Yes;
        case IDNO:
            return ErrorDialogResult::No;
        default:
            return ErrorDialogResult::Cancel;
    }
#endif
}

void Platform::Break()
{
#ifdef _WIN32
    __debugbreak();
#else
    throw std::runtime_error("Break");
#endif
}

#pragma region Thread
#ifdef _WIN32
#include <windows.h>
const DWORD MS_VC_EXCEPTION=0x406D1388;

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)


void SetThreadName(uint32_t dwThreadID, const char* threadName)
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;

    __try
    {
        RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }
}
void SetThreadName( const char* threadName)
{
    SetThreadName(GetCurrentThreadId(),threadName);
}

void SetThreadName( std::thread* thread, const char* threadName)
{
    DWORD threadId = ::GetThreadId( static_cast<HANDLE>( thread->native_handle() ) );
    SetThreadName(threadId,threadName);
}

#elif defined(__linux__)
#include <sys/prctl.h>
void SetThreadName( const char* threadName)
{
    prctl(PR_SET_NAME,threadName,0,0,0);
}

#else
void SetThreadName(std::thread* thread, const char* threadName)
{
    auto handle = thread->native_handle();
    pthread_setname_np(handle,threadName);
}
#endif

void Platform::SetThreadName(uint32_t threadId, const char* name)
{
#ifdef _WIN32
    ::SetThreadName(threadId, name);
#endif
}
#pragma endregion 