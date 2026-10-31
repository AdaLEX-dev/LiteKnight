// Soul Knight Linux — Logging System
#pragma once

#include "Types.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <fmt/core.h>
#include <fmt/color.h>
#include <mutex>
#include <chrono>
#include <fstream>
#include <filesystem>

namespace soulknight {

// Severity constants
constexpr uint8_t LogSeverity_TRACE = 0;
constexpr uint8_t LogSeverity_DEBUG = 1;
constexpr uint8_t LogSeverity_INFO  = 2;
constexpr uint8_t LogSeverity_WARN  = 3;
constexpr uint8_t LogSeverity_ERROR = 4;
constexpr uint8_t LogSeverity_FATAL = 5;

struct LogEntry {
    uint8_t level;
    std::string subsystem;
    std::string message;
    std::chrono::system_clock::time_point timestamp;
};

class Log {
public:
    static void Initialize(uint8_t min_level = 1);  // LogSeverity_DEBUG
    static void Shutdown();

    // Logging macros use these
    template<typename... Args>
    static void LogMessage(uint8_t level, std::string_view subsystem, fmt::format_string<Args...> fmt, Args&&... args) {
        if (level >= s_min_level) {
            std::string message = fmt::format(fmt, std::forward<Args>(args)...);
            LogMessageImpl(level, subsystem, message);
        }
    }

    static void LogMessage(uint8_t level, std::string_view subsystem, std::string_view message);
    static void LogMessage(uint8_t level, std::string_view message);

    static void SetMinLevel(uint8_t level);
    static void SetLogFile(const std::filesystem::path& path);
    static void SetConsoleOutput(bool enable);
    static void SetColorOutput(bool enable);

    static uint8_t GetMinLevel() { return s_min_level; }

private:
    static void LogMessageImpl(uint8_t level, std::string_view subsystem, std::string_view message);

    inline static uint8_t s_min_level = 1;  // LogSeverity_DEBUG
    inline static bool s_console_output = true;
    inline static bool s_color_output = true;
    inline static std::ofstream s_log_file;
    inline static std::mutex s_mutex;

    static std::string LevelToString(uint8_t level);
    static fmt::terminal_color LevelToColor(uint8_t level);
    static std::string FormatTimestamp();
};

// Subsystem prefixes
#define LOG_SUBSYSTEM_APP       "[APP]"
#define LOG_SUBSYSTEM_WAYLAND   "[WAYLAND]"
#define LOG_SUBSYSTEM_RENDER    "[RENDER]"
#define LOG_SUBSYSTEM_AUDIO     "[AUDIO]"
#define LOG_SUBSYSTEM_INPUT     "[INPUT]"
#define LOG_SUBSYSTEM_GAME      "[GAME]"
#define LOG_SUBSYSTEM_AI        "[AI]"
#define LOG_SUBSYSTEM_SAVE      "[SAVE]"
#define LOG_SUBSYSTEM_ASSET     "[ASSET]"
#define LOG_SUBSYSTEM_RE        "[RE]"
#define LOG_SUBSYSTEM_PERF      "[PERF]"

// Logging macros (compile-time level filtering)
#if defined(DEBUG) || defined(_DEBUG)
    #define LOG_TRACE(subsystem, ...) \
        do { if (soulknight::Log::GetMinLevel() <= 0) \
            soulknight::Log::LogMessage(0, subsystem __VA_OPT__(,) __VA_ARGS__); } while(0)
    #define LOG_DEBUG(subsystem, ...) \
        do { if (soulknight::Log::GetMinLevel() <= 1) \
            soulknight::Log::LogMessage(1, subsystem __VA_OPT__(,) __VA_ARGS__); } while(0)
#else
    #define LOG_TRACE(subsystem, ...) do {} while(0)
    #define LOG_DEBUG(subsystem, ...) do {} while(0)
#endif

#define LOG_INFO(subsystem, ...) \
    do { if (soulknight::Log::GetMinLevel() <= 2) \
        soulknight::Log::LogMessage(2, subsystem __VA_OPT__(,) __VA_ARGS__); } while(0)

#define LOG_WARN(subsystem, ...) \
    do { if (soulknight::Log::GetMinLevel() <= 3) \
        soulknight::Log::LogMessage(3, subsystem __VA_OPT__(,) __VA_ARGS__); } while(0)

#define LOG_ERROR(subsystem, ...) \
    do { if (soulknight::Log::GetMinLevel() <= 4) \
        soulknight::Log::LogMessage(4, subsystem __VA_OPT__(,) __VA_ARGS__); } while(0)

#define LOG_FATAL(subsystem, ...) \
    do { soulknight::Log::LogMessage(5, subsystem __VA_OPT__(,) __VA_ARGS__); } while(0)

// Convenience macros with subsystem
#define LOG_TRACE_APP(...)      LOG_TRACE(LOG_SUBSYSTEM_APP, __VA_ARGS__)
#define LOG_DEBUG_APP(...)      LOG_DEBUG(LOG_SUBSYSTEM_APP, __VA_ARGS__)
#define LOG_INFO_APP(...)       LOG_INFO(LOG_SUBSYSTEM_APP, __VA_ARGS__)
#define LOG_WARN_APP(...)       LOG_WARN(LOG_SUBSYSTEM_APP, __VA_ARGS__)
#define LOG_ERROR_APP(...)      LOG_ERROR(LOG_SUBSYSTEM_APP, __VA_ARGS__)
#define LOG_FATAL_APP(...)      LOG_FATAL(LOG_SUBSYSTEM_APP, __VA_ARGS__)

#define LOG_TRACE_INPUT(...)    LOG_TRACE(LOG_SUBSYSTEM_INPUT, __VA_ARGS__)
#define LOG_DEBUG_INPUT(...)    LOG_DEBUG(LOG_SUBSYSTEM_INPUT, __VA_ARGS__)
#define LOG_INFO_INPUT(...)     LOG_INFO(LOG_SUBSYSTEM_INPUT, __VA_ARGS__)
#define LOG_WARN_INPUT(...)     LOG_WARN(LOG_SUBSYSTEM_INPUT, __VA_ARGS__)
#define LOG_ERROR_INPUT(...)    LOG_ERROR(LOG_SUBSYSTEM_INPUT, __VA_ARGS__)
#define LOG_FATAL_INPUT(...)    LOG_FATAL(LOG_SUBSYSTEM_INPUT, __VA_ARGS__)

#define LOG_TRACE_AUDIO(...)    LOG_TRACE(LOG_SUBSYSTEM_AUDIO, __VA_ARGS__)
#define LOG_DEBUG_AUDIO(...)    LOG_DEBUG(LOG_SUBSYSTEM_AUDIO, __VA_ARGS__)
#define LOG_INFO_AUDIO(...)     LOG_INFO(LOG_SUBSYSTEM_AUDIO, __VA_ARGS__)
#define LOG_WARN_AUDIO(...)     LOG_WARN(LOG_SUBSYSTEM_AUDIO, __VA_ARGS__)
#define LOG_ERROR_AUDIO(...)    LOG_ERROR(LOG_SUBSYSTEM_AUDIO, __VA_ARGS__)
#define LOG_FATAL_AUDIO(...)    LOG_FATAL(LOG_SUBSYSTEM_AUDIO, __VA_ARGS__)

#define LOG_TRACE_ASSET(...)    LOG_TRACE(LOG_SUBSYSTEM_ASSET, __VA_ARGS__)
#define LOG_DEBUG_ASSET(...)    LOG_DEBUG(LOG_SUBSYSTEM_ASSET, __VA_ARGS__)
#define LOG_INFO_ASSET(...)     LOG_INFO(LOG_SUBSYSTEM_ASSET, __VA_ARGS__)
#define LOG_WARN_ASSET(...)     LOG_WARN(LOG_SUBSYSTEM_ASSET, __VA_ARGS__)
#define LOG_ERROR_ASSET(...)    LOG_ERROR(LOG_SUBSYSTEM_ASSET, __VA_ARGS__)
#define LOG_FATAL_ASSET(...)    LOG_FATAL(LOG_SUBSYSTEM_ASSET, __VA_ARGS__)

#define LOG_TRACE_GAME(...)     LOG_TRACE(LOG_SUBSYSTEM_GAME, __VA_ARGS__)
#define LOG_DEBUG_GAME(...)     LOG_DEBUG(LOG_SUBSYSTEM_GAME, __VA_ARGS__)
#define LOG_INFO_GAME(...)      LOG_INFO(LOG_SUBSYSTEM_GAME, __VA_ARGS__)
#define LOG_WARN_GAME(...)      LOG_WARN(LOG_SUBSYSTEM_GAME, __VA_ARGS__)
#define LOG_ERROR_GAME(...)     LOG_ERROR(LOG_SUBSYSTEM_GAME, __VA_ARGS__)
#define LOG_FATAL_GAME(...)     LOG_FATAL(LOG_SUBSYSTEM_GAME, __VA_ARGS__)

#define LOG_TRACE_PERF(...)     LOG_TRACE(LOG_SUBSYSTEM_PERF, __VA_ARGS__)
#define LOG_DEBUG_PERF(...)     LOG_DEBUG(LOG_SUBSYSTEM_PERF, __VA_ARGS__)
#define LOG_INFO_PERF(...)      LOG_INFO(LOG_SUBSYSTEM_PERF, __VA_ARGS__)
#define LOG_WARN_PERF(...)      LOG_WARN(LOG_SUBSYSTEM_PERF, __VA_ARGS__)
#define LOG_ERROR_PERF(...)     LOG_ERROR(LOG_SUBSYSTEM_PERF, __VA_ARGS__)
#define LOG_FATAL_PERF(...)     LOG_FATAL(LOG_SUBSYSTEM_PERF, __VA_ARGS__)

#define LOG_TRACE_RENDER(...)   LOG_TRACE(LOG_SUBSYSTEM_RENDER, __VA_ARGS__)
#define LOG_DEBUG_RENDER(...)   LOG_DEBUG(LOG_SUBSYSTEM_RENDER, __VA_ARGS__)
#define LOG_INFO_RENDER(...)    LOG_INFO(LOG_SUBSYSTEM_RENDER, __VA_ARGS__)
#define LOG_WARN_RENDER(...)    LOG_WARN(LOG_SUBSYSTEM_RENDER, __VA_ARGS__)
#define LOG_ERROR_RENDER(...)   LOG_ERROR(LOG_SUBSYSTEM_RENDER, __VA_ARGS__)
#define LOG_FATAL_RENDER(...)   LOG_FATAL(LOG_SUBSYSTEM_RENDER, __VA_ARGS__)

} // namespace soulknight