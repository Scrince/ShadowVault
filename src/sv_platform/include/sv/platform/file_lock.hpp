#pragma once

#include <sv/result.hpp>

#include <filesystem>
#include <memory>

namespace sv::platform {

/// RAII exclusive advisory lock on a container file.
class FileLock {
public:
    [[nodiscard]] static Result<FileLock> acquire(const std::filesystem::path& path);

    FileLock() noexcept = default;
    ~FileLock();

    FileLock(FileLock&& other) noexcept;
    FileLock& operator=(FileLock&& other) noexcept;

    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;

    [[nodiscard]] bool is_locked() const noexcept;
    [[nodiscard]] const std::filesystem::path& path() const noexcept;

private:
    struct Impl;
    explicit FileLock(std::unique_ptr<Impl> impl) noexcept;

    std::unique_ptr<Impl> impl_;
};

} // namespace sv::platform