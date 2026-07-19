#pragma once

#include <sv/mount/mount_types.hpp>
#include <sv/result.hpp>
#include <sv/vault/session_store.hpp>
#include <sv/vault/vault_engine.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace sv::mount {

struct SvFsAttr {
    bool is_directory = false;
    std::uint64_t size = 0;
    std::uint64_t mtime_unix = 0;
};


class SvFs {
public:
    using IoTouchCallback = std::function<void()>;

    SvFs(
        vault::VaultEngine& engine,
        vault::UnlockedVault& session,
        MountOptions options,
        IoTouchCallback on_io = {});

    [[nodiscard]] Result<SvFsAttr> getattr(const std::filesystem::path& fuse_path) const;
    [[nodiscard]] Result<std::vector<vault::VaultEntry>> readdir(const std::filesystem::path& fuse_path) const;

    [[nodiscard]] Result<int> open(const std::filesystem::path& fuse_path, bool write_requested);
    [[nodiscard]] Result<void> release(int handle);
    [[nodiscard]] Result<std::size_t> read(int handle, std::uint64_t offset, Span<std::uint8_t> buffer);
    [[nodiscard]] Result<std::size_t> write(int handle, std::uint64_t offset, Span<const std::uint8_t> buffer);
    [[nodiscard]] Result<void> flush(int handle);

    [[nodiscard]] Result<void> create_file(const std::filesystem::path& fuse_path);
    [[nodiscard]] Result<void> mkdir(const std::filesystem::path& fuse_path);
    [[nodiscard]] Result<void> unlink(const std::filesystem::path& fuse_path);
    [[nodiscard]] Result<void> rmdir(const std::filesystem::path& fuse_path);
    [[nodiscard]] Result<void> rename(
        const std::filesystem::path& from_path,
        const std::filesystem::path& to_path);
    [[nodiscard]] Result<void> truncate(const std::filesystem::path& fuse_path, std::uint64_t size);

    [[nodiscard]] bool read_only() const noexcept { return options_.read_only; }

private:
    struct OpenHandle {
        std::filesystem::path vault_path;
        std::vector<std::uint8_t> data;
        bool dirty = false;
    };

    [[nodiscard]] std::filesystem::path to_vault_path(const std::filesystem::path& fuse_path) const;
    [[nodiscard]] Result<SvFsAttr> getattr_vault_path(const std::filesystem::path& vault_path) const;
    void touch_io() const;

    vault::VaultEngine& engine_;
    vault::UnlockedVault& session_;
    MountOptions options_;
    IoTouchCallback on_io_;
    mutable std::mutex mutex_;
    std::unordered_map<int, OpenHandle> open_handles_;
    int next_handle_ = 1;
};

[[nodiscard]] int result_to_errno(const Result<void>& result) noexcept;
[[nodiscard]] int result_to_errno(const Result<std::size_t>& result) noexcept;

} 
