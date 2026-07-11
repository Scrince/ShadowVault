#pragma once

#include <sv/result.hpp>

#include <cstdint>
#include <filesystem>

namespace sv::platform {

/// Directory containing the running executable (e.g. svcli.exe).
[[nodiscard]] Result<std::filesystem::path> executable_directory();

/// Portable: directory beside executable; otherwise OS application-data location.
[[nodiscard]] Result<std::filesystem::path> config_directory(bool portable);

/// OS temporary directory.
[[nodiscard]] Result<std::filesystem::path> temp_directory();

/// Lexically normalize a path and apply platform-specific cleanup.
[[nodiscard]] Result<std::filesystem::path> normalize_path(std::filesystem::path path);

/// True when the path resides on removable media (USB, CD-ROM, etc.).
[[nodiscard]] Result<bool> is_removable_drive(const std::filesystem::path& path);

// ---------------------------------------------------------------------------
// Legacy helpers retained for portable/traveler integration (M1 stubs).
// ---------------------------------------------------------------------------

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

} // namespace sv::platform