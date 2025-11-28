#pragma once
#include <iostream>

namespace Platform
{
    enum class PlatformType
    {
        Windows,
        Linux,
        MacOS
    };
    
    enum DialogOption
    {
        Ok,                
        OkCancel,
        AbortRetryIgnore,
        YesNoCancel,
        YesNo,
        RetryCancel,
    };
    
    enum class ErrorDialogResult
    {
        Ok,
        Cancel,
        Abort,
        Retry,
        Ignore,
        Yes,
        No
    };
    
    int Snprintf(char* buffer, size_t size, const char* format, ...);
    
    ErrorDialogResult CreateErrorDialog(const char* title, const char* message, DialogOption option = DialogOption::OkCancel);
    
    void Break();
    
    void SetThreadName(uint32_t threadId, const char* name);
};
