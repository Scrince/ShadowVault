#pragma once

#include <sv/mount/backends/mount_backend.hpp>

namespace sv::mount {

/// Placeholder backend until WinFsp (Windows) or FUSE (Linux) is integrated.
class StubBackend final : public MountBackend {
public:
    [[nodiscard]] std::string name() const override;

    [[nodiscard]] Result<void> mount(
        const std::filesystem::path& vault_path,
        const MountOptions& opts) override;

    [[nodiscard]] Result<void> dismount(const std::filesystem::path& mount_point) override;

    [[nodiscard]] std::vector<MountSession> list_active() const override;
};

} // namespace sv::mount