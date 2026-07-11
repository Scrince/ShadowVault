#include <sv/platform/paths.hpp>

#include "paths_detail.hpp"

#include <algorithm>
#include <system_error>

namespace sv::platform {

namespace {

std::string path_to_string(const std::filesystem::path& path) {
    return path.string();
}

} // namespace

Result<std::filesystem::path> executable_directory() {
    return detail::query_executable_directory();
}

Result<std::filesystem::path> config_directory(const bool portable) {
    if (portable) {
        return executable_directory();
    }
    return detail::query_host_config_directory();
}

Result<std::filesystem::path> temp_directory() {
    return detail::query_temp_directory();
}

Result<std::filesystem::path> normalize_path(std::filesystem::path path) {
    if (path.empty()) {
        return Result<std::filesystem::path>::err(
            ErrorCode::InvalidArgument,
            "path must not be empty");
    }

    std::error_code ec;
    path = path.lexically_normal();

#if defined(_WIN32)
    auto normalized = path_to_string(path);
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    path = std::filesystem::path(normalized);
#endif

    if (path.has_relative_path()) {
        const auto filename = path.filename().string();
        if (!filename.empty() && filename != "." && filename != "..") {
            // Keep lexical form; avoid canonicalize failures on missing paths.
            return Result<std::filesystem::path>::ok(std::move(path));
        }
    }

    if (ec) {
        return Result<std::filesystem::path>::err(ErrorCode::IoError, ec.message());
    }

    return Result<std::filesystem::path>::ok(std::move(path));
}

Result<bool> is_removable_drive(const std::filesystem::path& path) {
    if (path.empty()) {
        return Result<bool>::err(ErrorCode::InvalidArgument, "path must not be empty");
    }
    return detail::query_is_removable_drive(path);
}

ResolvedPaths default_paths() {
    ResolvedPaths paths;

    if (const auto host_config = config_directory(false); host_config.is_ok()) {
        paths.config_dir = host_config.value();
        paths.config_file = host_config.value() / "config.ini";
        paths.cache_dir = host_config.value() / "cache";
        paths.logs_dir = host_config.value() / "logs";
        paths.recent_volumes_file = host_config.value() / "recent.json";
    } else if (const auto exe = executable_directory(); exe.is_ok()) {
        paths.config_dir = exe.value() / "ShadowVault";
        paths.config_file = paths.config_dir / "config.ini";
        paths.cache_dir = paths.config_dir / "cache";
        paths.logs_dir = paths.config_dir / "logs";
        paths.recent_volumes_file = paths.config_dir / "recent.json";
    }

    if (const auto temp = temp_directory(); temp.is_ok()) {
        paths.temp_dir = temp.value() / "ShadowVault";
    } else {
        std::error_code ec;
        const auto fallback_temp = std::filesystem::temp_directory_path(ec);
        if (!ec) {
            paths.temp_dir = fallback_temp / "ShadowVault";
        }
    }

    if (paths.config_dir.empty()) {
        paths.config_dir = paths.temp_dir.empty() ? std::filesystem::path{"."} : paths.temp_dir;
        paths.config_file = paths.config_dir / "config.ini";
        paths.cache_dir = paths.config_dir / "cache";
        paths.logs_dir = paths.config_dir / "logs";
        paths.recent_volumes_file = paths.config_dir / "recent.json";
    }

    return paths;
}

bool path_exists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool is_directory(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

std::uintmax_t count_regular_files(const std::filesystem::path& directory) {
    if (!std::filesystem::is_directory(directory)) {
        return 0;
    }

    std::uintmax_t count = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_regular_file(ec)) {
            ++count;
        }
    }
    return count;
}

} // namespace sv::platform