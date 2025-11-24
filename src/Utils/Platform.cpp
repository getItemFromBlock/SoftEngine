#include "Platform.h"

#include <cstdarg>
#include <cstdio>

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
