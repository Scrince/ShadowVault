#pragma once

#include <sv/result.hpp>

#include <filesystem>

namespace sv::platform::detail {

[[nodiscard]] Result<std::filesystem::path> query_executable_directory();
[[nodiscard]] Result<std::filesystem::path> query_host_config_directory();
[[nodiscard]] Result<std::filesystem::path> query_temp_directory();
[[nodiscard]] Result<bool> query_is_removable_drive(const std::filesystem::path& path);

} // namespace sv::platform::detail