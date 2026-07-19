#include <sv/logger.hpp>

#include <iostream>
#include <mutex>

namespace sv {

namespace {

std::mutex g_log_mutex;
LogLevel g_log_level = LogLevel::Info;

constexpr const char* level_name(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Error:
        return "error";
    case LogLevel::Warn:
        return "warn";
    case LogLevel::Info:
        return "info";
    case LogLevel::Debug:
        return "debug";
    }
    return "unknown";
}

void log_message(LogLevel level, std::string_view component, std::string_view message) {
    if (level > g_log_level) {
        return;
    }

    const std::lock_guard lock(g_log_mutex);
    std::cerr << '[' << level_name(level) << "] " << component << ": " << message << '\n';
}

} 

void set_log_level(LogLevel level) noexcept {
    g_log_level = level;
}

LogLevel log_level() noexcept {
    return g_log_level;
}

void log_error(std::string_view component, std::string_view message) {
    log_message(LogLevel::Error, component, message);
}

void log_warn(std::string_view component, std::string_view message) {
    log_message(LogLevel::Warn, component, message);
}

void log_info(std::string_view component, std::string_view message) {
    log_message(LogLevel::Info, component, message);
}

void log_debug(std::string_view component, std::string_view message) {
    log_message(LogLevel::Debug, component, message);
}

} 