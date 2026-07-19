#if !defined(_WIN32)

#include "../paths_detail.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <pwd.h>
#include <string>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/syslimits.h>
#else
#include <climits>
#endif

namespace sv::platform::detail {

namespace {

std::filesystem::path home_directory() {
    if (const char* home = std::getenv("HOME"); home != nullptr && home[0] != '\0') {
        return std::filesystem::path(home);
    }

    if (passwd* entry = getpwuid(getuid()); entry != nullptr && entry->pw_dir != nullptr) {
        return std::filesystem::path(entry->pw_dir);
    }

    return std::filesystem::current_path();
}

Result<std::filesystem::path> errno_error(const char* context) {
    return Result<std::filesystem::path>::err(
        ErrorCode::PlatformError,
        std::string(context) + ": " + std::strerror(errno));
}

bool is_known_removable_mount(const std::filesystem::path& path) {
    const auto native = path.lexically_normal().string();
    return native.rfind("/media/", 0) == 0
        || native.rfind("/run/media/", 0) == 0
        || native.rfind("/mnt/", 0) == 0
        || native.rfind("/Volumes/", 0) == 0;
}

} 

Result<std::filesystem::path> query_executable_directory() {
#if defined(__linux__)
    std::string buffer(PATH_MAX, '\0');
    while (true) {
        const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (length < 0) {
            return errno_error("readlink /proc/self/exe failed");
        }
        if (static_cast<std::size_t>(length) < buffer.size()) {
            buffer.resize(static_cast<std::size_t>(length));
            return Result<std::filesystem::path>::ok(
                std::filesystem::path(buffer).parent_path());
        }
        buffer.resize(buffer.size() * 2);
    }
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    if (_NSGetExecutablePath(nullptr, &size) != -1) {
        return Result<std::filesystem::path>::err(
            ErrorCode::PlatformError,
            "_NSGetExecutablePath size query failed");
    }

    std::string buffer(size, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        return errno_error("_NSGetExecutablePath failed");
    }

    std::error_code ec;
    const auto canonical = std::filesystem::weakly_canonical(std::filesystem::path(buffer.data()), ec);
    if (ec) {
        return Result<std::filesystem::path>::err(ErrorCode::PlatformError, ec.message());
    }
    return Result<std::filesystem::path>::ok(canonical.parent_path());
#else
    return Result<std::filesystem::path>::ok(std::filesystem::current_path());
#endif
}

Result<std::filesystem::path> query_host_config_directory() {
#if defined(__APPLE__)
    return Result<std::filesystem::path>::ok(
        home_directory() / "Library" / "Application Support" / "ShadowVault");
#else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg != nullptr && xdg[0] != '\0') {
        return Result<std::filesystem::path>::ok(std::filesystem::path(xdg) / "shadowvault");
    }
    return Result<std::filesystem::path>::ok(home_directory() / ".config" / "shadowvault");
#endif
}

Result<std::filesystem::path> query_temp_directory() {
    if (const char* tmp = std::getenv("TMPDIR"); tmp != nullptr && tmp[0] != '\0') {
        return Result<std::filesystem::path>::ok(std::filesystem::path(tmp));
    }

    std::error_code ec;
    const auto temp = std::filesystem::temp_directory_path(ec);
    if (ec) {
        return Result<std::filesystem::path>::err(ErrorCode::PlatformError, ec.message());
    }
    return Result<std::filesystem::path>::ok(temp);
}

Result<bool> query_is_removable_drive(const std::filesystem::path& path) {
    return Result<bool>::ok(is_known_removable_mount(path));
}

} 

#endif