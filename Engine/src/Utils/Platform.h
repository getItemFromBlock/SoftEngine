#pragma once
#include <iostream>
#include <thread>
#include <vector>
#include <filesystem>

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
    
    struct Filter
    {
        Filter(std::string _name, std::string _spec) : name(std::move(_name)), spec(std::move(_spec)) {}
			
        std::string name;
        // ex : "Text file"
        std::string spec;
        // ex : "txt"
    };
    
    int Snprintf(char* buffer, size_t size, const char* format, ...);
    
    ErrorDialogResult CreateErrorDialog(const char* title, const char* message, DialogOption option = DialogOption::OkCancel);
		
    std::string SaveDialog(const std::vector<Filter>& filters, const std::filesystem::path& defaultOpenPath = "");

    std::string OpenDialog(const std::vector<Filter>& filters, const std::filesystem::path& defaultOpenPath = "");
    
    void Break();
    
    void SetThreadName(uint32_t threadId, const char* name);
    void SetThreadName(std::thread::native_handle_type threadId, const char* name);
};
