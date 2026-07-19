#pragma once

#include <sv/container/container_file.hpp>
#include <sv/crypto/crypto_provider.hpp>
#include <sv/result.hpp>
#include <sv/span.hpp>

namespace sv::recovery {

struct RepairOptions {
    bool dry_run = false;
    bool from_backup = false;
    bool replay_journal = false;
};

[[nodiscard]] Result<void> repair_from_backup_header(
    container::ContainerFileStore& container,
    RepairOptions options = {}) noexcept;

[[nodiscard]] Result<void> repair_replay_journal(
    container::ContainerFileStore& container,
    crypto::CryptoProvider& crypto,
    Span<const std::uint8_t> vmk,
    container::Superblock& superblock,
    RepairOptions options = {}) noexcept;

[[nodiscard]] Result<void> repair_vault(
    container::ContainerFileStore& container,
    crypto::CryptoProvider& crypto,
    Span<const std::uint8_t> vmk,
    RepairOptions options = {}) noexcept;

} 