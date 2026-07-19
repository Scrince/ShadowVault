#pragma once

#include <sv/result.hpp>

#include <filesystem>
#include <memory>

namespace sv::platform::detail {

struct FileLockState {
    std::filesystem::path path;

#if defined(_WIN32)
    void* file_handle{nullptr};
#else
    int fd{-1};
#endif

    ~FileLockState();
};

[[nodiscard]] Result<std::unique_ptr<FileLockState>> open_exclusive_lock(
    const std::filesystem::path& path);

} 