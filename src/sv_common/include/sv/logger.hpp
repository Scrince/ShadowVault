#pragma once

#include <string>
#include <string_view>

namespace sv {

enum class LogLevel {
    Error = 0,
    Warn = 1,
    Info = 2,
    Debug = 3,
};


void set_log_level(LogLevel level) noexcept;

[[nodiscard]] LogLevel log_level() noexcept;


void log_error(std::string_view component, std::string_view message);
void log_warn(std::string_view component, std::string_view message);
void log_info(std::string_view component, std::string_view message);
void log_debug(std::string_view component, std::string_view message);

} 