#pragma once
#include "EngineAPI.h"

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <cassert>
#include <cstdio>
#include <ctime>
#include <cstdint>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define MAX_LOG_SIZE UINT16_MAX
#define LOG_PATH "Logs/"

namespace Debug
{
    template <typename... Args>
    static std::string FormatString(const char* format, const Args&... args)
    {
        static_assert((std::conjunction_v<std::bool_constant<std::is_arithmetic_v<Args> || std::is_pointer_v<Args>> ...>),
                      "FormatString: arguments must be arithmetic or pointer types (printf-compatible)");

        int size = std::snprintf(nullptr, 0, format, args...);
        if (size < 0) return {};

        std::string buffer(size + 1, '\0');
        std::snprintf(buffer.data(), buffer.size(), format, args...);
        buffer.resize(size);

        return buffer;
    }
    
    enum class LogType
    {
        L_INFO,
        L_WARNING,
        L_ERROR
    };

    inline const char* SerializeLogTypeValue(LogType value)
    {
        switch (value)
        {
        default:
        case LogType::L_INFO: return "Info";
        case LogType::L_WARNING: return "Warning";
        case LogType::L_ERROR: return "Error";
        }
    }

    class ENGINE_API Log
    {
    public:
        ~Log() = default;

        static bool LogToFile;

        static void OpenFile(const tm& calendar_time);

        static void WriteToFile(LogType type, const tm& calendar_time, const char* messageAndFile);

        static void CloseFile();
        
        static void ShouldBreak(const std::string& message);

        template <typename... Args>
        static void Print(const char* file, int line, LogType type, const char* format, Args... args)
        {
            time_t now = std::time(nullptr);
            tm calendar_time;

        #ifdef _WIN32
            localtime_s(&calendar_time, &now);
        #elif defined(__linux__)
            localtime_r(&now, &calendar_time);
        #endif

            if (LogToFile && !m_isFileOpen)
                OpenFile(calendar_time);

            std::string message = FormatString(format, args...);
            std::string fileLine = FormatString("%s (l:%d): ", file, line);
            std::string messageAndFile = fileLine + message + "\n";
            std::string header = FormatString("[%02d:%02d:%02d] ", calendar_time.tm_hour, calendar_time.tm_min, calendar_time.tm_sec);
            std::string result = header + messageAndFile;

        #ifdef _WIN32
            const HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, 10); // Green for header
            std::cout << header;
            SetConsoleTextAttribute(hConsole, 14); // Yellow for fileLine
            std::cout << fileLine;
            switch (type) {
                case LogType::L_INFO: SetConsoleTextAttribute(hConsole, 15); break;
                case LogType::L_WARNING: SetConsoleTextAttribute(hConsole, 14); break;
                case LogType::L_ERROR: SetConsoleTextAttribute(hConsole, 4); break;
                default: break;
            }
            std::cout << message << std::endl;
            SetConsoleTextAttribute(hConsole, 15); // Reset
        #else
            switch (type) {
                case LogType::L_INFO: std::cout << "\033[37m"; break;
                case LogType::L_WARNING: std::cout << "\033[33m"; break;
                case LogType::L_ERROR: std::cout << "\033[31m"; break;
                default: break;
            }
            std::cout << result << "\033[0m"; // Reset
        #endif

            if (LogToFile && m_isFileOpen)
                WriteToFile(type, calendar_time, messageAndFile.c_str());

        #ifndef NDEBUG
            if (type == LogType::L_ERROR)
                ShouldBreak(message);
        #endif
        }

    private:
        static bool m_isFileOpen;
        static std::ofstream m_file;
    };
}

#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define LOG(t, x, ...) Debug::Log::Print(__FILENAME__, __LINE__, t, x, ##__VA_ARGS__)
#define PrintLog(x, ...) Debug::Log::Print(__FILENAME__, __LINE__, Debug::LogType::L_INFO, x, ##__VA_ARGS__)
#define PrintWarning(x, ...) Debug::Log::Print(__FILENAME__, __LINE__, Debug::LogType::L_WARNING, x, ##__VA_ARGS__)
#define PrintError(x, ...) Debug::Log::Print(__FILENAME__, __LINE__, Debug::LogType::L_ERROR, x, ##__VA_ARGS__)

#define UNUSED(x) (void)(x)
#define ASSERT(x) if (!(x)) { PrintError("Assertion failed: %s", #x); }
