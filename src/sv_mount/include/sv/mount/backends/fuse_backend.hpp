#pragma once

#include <sv/mount/backends/mount_backend.hpp>

namespace sv::mount {

/// FUSE3 mount backend (WinFsp on Windows, libfuse3 on Linux).
class FuseMountBackend final : public MountBackend {
public:
    FuseMountBackend();
    ~FuseMountBackend() override;

    [[nodiscard]] std::string name() const override;

    [[nodiscard]] Result<void> mount(
        const std::filesystem::path& vault_path,
        const MountOptions& opts) override;

    [[nodiscard]] Result<void> dismount(const std::filesystem::path& mount_point) override;

    [[nodiscard]] std::vector<MountSession> list_active() const override;
};

} // namespace sv::mount