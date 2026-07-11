#pragma once

#include <sv/result.hpp>
#include <sv/vault/session_store.hpp>

namespace sv::vault {

[[nodiscard]] bool hidden_configured_from_header(const container::HeaderPayload& header) noexcept;
[[nodiscard]] bool hidden_configured_from_manifest(const ManifestState& manifest) noexcept;
[[nodiscard]] void update_hidden_configured(UnlockedVault& session) noexcept;

[[nodiscard]] Result<void> check_write_allowed(const UnlockedVault& session, std::uint64_t write_end) noexcept;

class VaultEngine;

[[nodiscard]] Result<void> create_hidden_volume(
    VaultEngine& engine,
    UnlockedVault& outer_session,
    const CredentialMaterial& hidden_cred,
    std::uint64_t hidden_max_size) noexcept;

} // namespace sv::vault