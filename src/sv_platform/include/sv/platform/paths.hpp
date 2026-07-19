#pragma once

#include <sv/result.hpp>

#include <cstdint>
#include <filesystem>

namespace sv::platform {


[[nodiscard]] Result<std::filesystem::path> executable_directory();


[[nodiscard]] Result<std::filesystem::path> config_directory(bool portable);


[[nodiscard]] Result<std::filesystem::path> temp_directory();


[[nodiscard]] Result<std::filesystem::path> normalize_path(std::filesystem::path path);


[[nodiscard]] Result<bool> is_removable_drive(const std::filesystem::path& path);


struct ResolvedPaths {
    std::filesystem::path config_file;
    std::filesystem::path config_dir;
    std::filesystem::path temp_dir;
    std::filesystem::path cache_dir;
    std::filesystem::path logs_dir;
    std::filesystem::path recent_volumes_file;
};

[[nodiscard]] ResolvedPaths default_paths();
[[nodiscard]] bool path_exists(const std::filesystem::path& path);
[[nodiscard]] bool is_directory(const std::filesystem::path& path);
[[nodiscard]] std::uintmax_t count_regular_files(const std::filesystem::path& directory);

} 