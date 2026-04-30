#include "Platform.h"

#include <cstdarg>
#include <cstdio>
#include <thread>
#include <cstdint>

#include "Core/Window.h"

#include <nfd.hpp>

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

std::string Platform::SaveDialog(const std::vector<Filter>& filters,
    const std::filesystem::path& defaultOpenPath)
{
    std::string resultString;

    NFD::Guard nfdGuard;

    NFD::UniquePath outPath;

    const size_t count = filters.size();
    std::vector<nfdfilteritem_t> filterItems(count);

    for (size_t i = 0; i < count; i++)
    {
        filterItems[i].name = filters[i].name.c_str();
        filterItems[i].spec = filters[i].spec.c_str();
    }

    // show the dialog
    const nfdresult_t result = NFD::SaveDialog(outPath, filterItems.data(), static_cast<uint32_t>(count),
                                               defaultOpenPath.generic_string().c_str());
    if (result == NFD_OKAY)
    {
        resultString = std::string(outPath.get());
    }

    // NFD::Guard will automatically quit NFD.
    return resultString;
}

std::string Platform::OpenDialog(const std::vector<Filter>& filters,
    const std::filesystem::path& defaultOpenPath)
{
    std::string resultString;

    // initialize NFD
    NFD::Guard nfdGuard;

    // auto-freeing memory
    NFD::UniquePath outPath;

    const size_t count = filters.size();
    // prepare filters for the dialog
    std::vector<nfdfilteritem_t> filterItems(count);

    for (size_t i = 0; i < count; i++)
    {
        filterItems[i].name = filters[i].name.c_str();
        filterItems[i].spec = filters[i].spec.c_str();
    }

    // show the dialog
    const nfdresult_t result = NFD::OpenDialog(outPath, filterItems.data(), static_cast<uint32_t>(count),
                                               defaultOpenPath.generic_string().c_str());
    if (result == NFD_OKAY)
    {
        resultString = std::string(outPath.get());
    }
    
    // NFD::Guard will automatically quit NFD.
    return resultString;
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

#if defined(_MSC_VER)

const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push, 8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType;
    LPCSTR szName;
    DWORD dwThreadID;
    DWORD dwFlags;
} THREADNAME_INFO;
#pragma pack(pop)

static void SetThreadNameImpl(DWORD threadId, const char* name)
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = name;
    info.dwThreadID = threadId;
    info.dwFlags = 0;

    __try
    {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }
}

#elif defined(__MINGW32__)

static void SetThreadNameImpl(DWORD threadId, const char* name)
{
    // Win10+ only
    typedef HRESULT (WINAPI *SetThreadDescriptionFunc)(HANDLE, PCWSTR);
    HMODULE hKernel = GetModuleHandleA("Kernel32.dll");
    auto func = (SetThreadDescriptionFunc)GetProcAddress(hKernel, "SetThreadDescription");

    if (func)
    {
        int len = MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
        std::wstring wname(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, name, -1, &wname[0], len);
        func(OpenThread(THREAD_SET_LIMITED_INFORMATION, FALSE, threadId), wname.c_str());
    }
}

#elif defined(__linux__)

#include <sys/prctl.h>

static void SetThreadNameImpl(const char* name)
{
    prctl(PR_SET_NAME, name, 0, 0, 0);
}

#elif defined(__APPLE__)

static void SetThreadNameImpl(const char* name)
{
    pthread_setname_np(name);
}

#endif


void Platform::SetThreadName(uint32_t threadId, const char* name)
{
#if defined(_MSC_VER) || defined(__MINGW32__)
    SetThreadNameImpl(threadId, name);
#elif defined(__linux__) || defined(__APPLE__)
    SetThreadNameImpl(name);
#endif
}

void Platform::SetThreadName(std::thread::native_handle_type threadId, const char *name)
{
#if defined(_MSC_VER) || defined(__MINGW32__)
    // Will not work for non ascii stuff but eh it'll be fine
    std::string str = name;
    std::wstring stemp = std::wstring(str.begin(), str.end());
    SetThreadDescription(threadId, stemp.c_str());
#elif defined(__linux__) || defined(__APPLE__)
    SetThreadNameImpl(name);
#endif
}

#pragma endregion

