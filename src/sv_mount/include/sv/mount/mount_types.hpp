#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>

namespace sv::mount {

using MountSessionId = std::uint64_t;

struct CachePolicy {
    std::size_t max_bytes{256 * 1024 * 1024};
    bool drop_on_lock{true};
};

struct MountOptions {
    std::filesystem::path mount_point;
    bool read_only{false};
    bool protect_hidden{false};
    bool auto_dismount{true};
    std::chrono::seconds idle_timeout{900};
    CachePolicy cache{};
};

struct MountSession {
    MountSessionId session_id{0};
    std::filesystem::path mount_point;
    std::filesystem::path vault_path;
    bool read_only{false};
    bool auto_dismount{true};
    std::chrono::seconds idle_timeout{900};
    std::chrono::system_clock::time_point last_io_time{};
};

} 