#pragma once

#include <sv/mount/mount_types.hpp>
#include <sv/result.hpp>
#include <sv/vault/chunk_store.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace sv::mount {

class MountBackend {
public:
    virtual ~MountBackend() = default;

    [[nodiscard]] virtual std::string name() const = 0;

    [[nodiscard]] virtual Result<void> mount(
        const std::filesystem::path& vault_path,
        const MountOptions& opts) = 0;

    [[nodiscard]] virtual Result<void> dismount(const std::filesystem::path& mount_point) = 0;

    [[nodiscard]] virtual std::vector<MountSession> list_active() const = 0;
};

} // namespace sv::mount