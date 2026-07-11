#pragma once

#include <sv/result.hpp>
#include <sv/vault/session_store.hpp>
#include <sv/vault/vault_engine.hpp>

namespace sv::vault {

class FileOps {
public:
    explicit FileOps(VaultEngine& engine) noexcept;

    [[nodiscard]] Result<void> put(
        UnlockedVault& session,
        const std::filesystem::path& vault_path,
        Span<const std::uint8_t> data,
        bool force = false);

    [[nodiscard]] Result<std::vector<std::uint8_t>> get(
        const UnlockedVault& session,
        const std::filesystem::path& vault_path) const;

    [[nodiscard]] Result<std::vector<VaultEntry>> list(
        const UnlockedVault& session,
        const std::filesystem::path& vault_dir) const;

    [[nodiscard]] Result<void> remove(UnlockedVault& session, const std::filesystem::path& vault_path);
    [[nodiscard]] Result<void> rename(
        UnlockedVault& session,
        const std::filesystem::path& from_path,
        const std::filesystem::path& to_path);
    [[nodiscard]] Result<void> mkdir(UnlockedVault& session, const std::filesystem::path& vault_dir);

private:
    VaultEngine& engine_;
};

} // namespace sv::vault