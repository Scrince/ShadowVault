#include <sv/vault/file_ops.hpp>

namespace sv::vault {

FileOps::FileOps(VaultEngine& engine) noexcept : engine_(engine) {}

Result<void> FileOps::put(
    UnlockedVault& session,
    const std::filesystem::path& vault_path,
    const Span<const std::uint8_t> data,
    const bool force) {
    return engine_.put(session, vault_path, data, force);
}

Result<std::vector<std::uint8_t>> FileOps::get(
    const UnlockedVault& session,
    const std::filesystem::path& vault_path) const {
    return engine_.get(session, vault_path);
}

Result<std::vector<VaultEntry>> FileOps::list(
    const UnlockedVault& session,
    const std::filesystem::path& vault_dir) const {
    return engine_.list(session, vault_dir);
}

Result<void> FileOps::remove(UnlockedVault& session, const std::filesystem::path& vault_path) {
    return engine_.remove(session, vault_path);
}

Result<void> FileOps::rename(
    UnlockedVault& session,
    const std::filesystem::path& from_path,
    const std::filesystem::path& to_path) {
    return engine_.rename(session, from_path, to_path);
}

Result<void> FileOps::mkdir(UnlockedVault& session, const std::filesystem::path& vault_dir) {
    return engine_.mkdir(session, vault_dir);
}

} // namespace sv::vault