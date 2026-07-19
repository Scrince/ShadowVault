#pragma once

#include <sv/container/container_file.hpp>
#include <sv/result.hpp>
#include <sv/vault/credential_material.hpp>
#include <sv/vault/session_store.hpp>

namespace sv::vault {

struct VerifyReport {
    bool structural_ok = false;
    bool crypto_ok = false;
    std::string message;
};

[[nodiscard]] Result<VerifyReport> verify_structural(container::ContainerFileStore& container) noexcept;

[[nodiscard]] Result<VerifyReport> verify_vault(
    container::ContainerFileStore& container,
    const std::optional<CredentialMaterial>& cred = std::nullopt) noexcept;

[[nodiscard]] Result<VerifyReport> verify_session(
    const UnlockedVault& session,
    bool deep = false) noexcept;

} 