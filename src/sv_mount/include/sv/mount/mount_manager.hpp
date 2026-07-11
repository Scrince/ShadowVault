#pragma once

#include <sv/mount/backends/mount_backend.hpp>
#include <sv/mount/mount_types.hpp>
#include <sv/result.hpp>

#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace sv::mount {

/// Process-wide mount session registry (M9 scaffold).
class MountManager {
public:
    static MountManager& instance();

    [[nodiscard]] Result<MountSessionId> mount_vault(
        const std::filesystem::path& vault_path,
        MountOptions opts);

    [[nodiscard]] Result<void> dismount(MountSessionId id);
    [[nodiscard]] Result<void> dismount_point(const std::filesystem::path& mount_point);
    [[nodiscard]] Result<void> dismount_all();

    [[nodiscard]] std::vector<MountSession> list_sessions() const;

    void on_idle_tick();
    void touch_io(MountSessionId id);

private:
    MountManager();
    [[nodiscard]] MountSessionId next_session_id();

    mutable std::mutex mutex_;
    std::unique_ptr<MountBackend> backend_;
    std::unordered_map<MountSessionId, MountSession> sessions_;
    MountSessionId next_id_{1};
};

} // namespace sv::mount