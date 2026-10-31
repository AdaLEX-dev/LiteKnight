// Soul Knight Linux — Logging Implementation
#include "Log.hpp"
#include <fmt/chrono.h>
#include <iostream>
#include <iomanip>
#include <sstream>

using namespace soulknight;

void Log::Initialize(uint8_t min_level) {
    s_min_level = min_level;
    s_console_output = true;
    s_color_output = true;
    LogMessage(2, "[APP]", "Logging initialized (min level: {})", static_cast<int>(min_level));
}

void Log::Shutdown() {
    if (s_log_file.is_open()) {
        s_log_file.flush();
        s_log_file.close();
    }
}

void Log::SetMinLevel(uint8_t level) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_min_level = level;
}

void Log::SetLogFile(const std::filesystem::path& path) {
    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_log_file.is_open()) {
        s_log_file.close();
    }
    s_log_file.open(path, std::ios::out | std::ios::app);
    if (s_log_file.is_open()) {
        LogMessage(2, "[APP]", "Log file opened: {}", path.string());
    }
}

void Log::SetConsoleOutput(bool enable) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_console_output = enable;
}

void Log::SetColorOutput(bool enable) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_color_output = enable;
}

void Log::LogMessage(uint8_t level, std::string_view subsystem, std::string_view message) {
    if (level < s_min_level) return;

    std::string formatted = fmt::format("[{}] {} {}: {}", FormatTimestamp(), LevelToString(level), subsystem, message);

    std::lock_guard<std::mutex> lock(s_mutex);

    if (s_console_output) {
        if (s_color_output) {
            fmt::print(fg(LevelToColor(level)), "{}\n", formatted);
        } else {
            std::cout << formatted << '\n';
        }
    }

    if (s_log_file.is_open()) {
        s_log_file << formatted << '\n';
        s_log_file.flush();
    }
}

void Log::LogMessage(uint8_t level, std::string_view message) {
    LogMessage(level, "[APP]", message);
}

std::string Log::LevelToString(uint8_t level) {
    switch (level) {
        case 0: return "TRACE";
        case 1: return "DEBUG";
        case 2: return "INFO ";
        case 3: return "WARN ";
        case 4: return "ERROR";
        case 5: return "FATAL";
    }
    return "UNKN";
}

fmt::terminal_color Log::LevelToColor(uint8_t level) {
    switch (level) {
        case 0: return fmt::terminal_color::bright_black;
        case 1: return fmt::terminal_color::cyan;
        case 2: return fmt::terminal_color::green;
        case 3: return fmt::terminal_color::yellow;
        case 4: return fmt::terminal_color::red;
        case 5: return fmt::terminal_color::bright_red;
    }
    return fmt::terminal_color::white;
}

std::string Log::FormatTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

void Log::LogMessageImpl(uint8_t level, std::string_view subsystem, std::string_view message) {
    if (level < s_min_level) return;

    std::string formatted = fmt::format("[{}] {} {}: {}", FormatTimestamp(), LevelToString(level), subsystem, message);

    std::lock_guard<std::mutex> lock(s_mutex);

    if (s_console_output) {
        if (s_color_output) {
            fmt::print(fg(LevelToColor(level)), "{}\n", formatted);
        } else {
            std::cout << formatted << '\n';
        }
    }

    if (s_log_file.is_open()) {
        s_log_file << formatted << '\n';
        s_log_file.flush();
    }
}