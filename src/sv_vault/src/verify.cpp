#include <sv/vault/verify.hpp>

#include <sv/container/layout.hpp>
#include <sv/vault/key_manager.hpp>
#include <sv/vault/chunk_store.hpp>
#include <sv/vault/manifest.hpp>

namespace sv::vault {

Result<VerifyReport> verify_structural(container::ContainerFileStore& container) noexcept {
    VerifyReport report {};

    const auto primary = container.read_superblock_primary();
    if (!primary.is_ok()) {
        report.message = primary.message();
        return Result<VerifyReport>::ok(report);
    }

    if (container.file_size() < primary.value().container_size) {
        report.message = "container file truncated";
        return Result<VerifyReport>::ok(report);
    }

    const auto tail = container.read_superblock_tail();
    if (!tail.is_ok()) {
        report.message = tail.message();
        return Result<VerifyReport>::ok(report);
    }

    if (primary.value().crc32 != tail.value().crc32) {
        report.message = "primary and tail superblock crc mismatch";
        return Result<VerifyReport>::ok(report);
    }

    if (!container::validate_layout(primary.value())) {
        report.message = "superblock layout validation failed";
        return Result<VerifyReport>::ok(report);
    }

    report.structural_ok = true;
    report.message = "structural checks passed";
    return Result<VerifyReport>::ok(report);
}

Result<VerifyReport> verify_session(const UnlockedVault& session, const bool deep) noexcept {
    VerifyReport report {};
    const auto structural = verify_structural(const_cast<container::ContainerFileStore&>(session.container));
    if (!structural.is_ok()) {
        return structural;
    }
    report = structural.value();
    if (!report.structural_ok) {
        return Result<VerifyReport>::ok(report);
    }

    if (session.header.manifest_root_hash != session.manifest.root_hash) {
        report.message = "manifest_root_hash mismatch";
        return Result<VerifyReport>::ok(report);
    }

    if (deep) {
        auto crypto = crypto::create_default_crypto_provider();
        if (!crypto) {
            return Result<VerifyReport>::err(ErrorCode::PlatformError, "crypto provider unavailable");
        }
        ChunkStore chunks(
            const_cast<container::ContainerFileStore&>(session.container),
            *crypto,
            session.header.data_offset,
            session.header.chunk_size);
        for (const auto& inode : session.manifest.inodes) {
            if (inode.entry_type != container::InodeEntryType::File || inode.chunk_count == 0) {
                continue;
            }
            const auto generation = inode.data_generation != 0
                ? inode.data_generation
                : session.manifest.generation;
            const auto data = chunks.read_file(inode, session.vmk.span(), generation);
            if (!data.is_ok()) {
                report.message = "deep verify failed for " + inode.name_utf8 + ": " + data.message();
                return Result<VerifyReport>::ok(report);
            }
        }
    }

    report.crypto_ok = true;
    report.message = deep ? "deep crypto checks passed" : "crypto checks passed";
    return Result<VerifyReport>::ok(report);
}

Result<VerifyReport> verify_vault(
    container::ContainerFileStore& container,
    const std::optional<CredentialMaterial>& cred) noexcept {
    auto report = verify_structural(container);
    if (!report.is_ok() || !report.value().structural_ok || !cred.has_value()) {
        return report;
    }

    auto crypto = crypto::create_default_crypto_provider();
    if (!crypto) {
        return Result<VerifyReport>::err(ErrorCode::PlatformError, "crypto provider unavailable");
    }

    const auto primary_sb = container.read_superblock_primary();
    if (!primary_sb.is_ok()) {
        return Result<VerifyReport>::err(primary_sb.error(), primary_sb.message());
    }

    KeyManager keys(*crypto);
    const auto derived = keys.derive_from_credentials(cred.value(), primary_sb.value(), HeaderSlotIntent::Outer);
    if (!derived.is_ok()) {
        report.value().message = derived.message();
        return report;
    }

    const auto header_sector = container.read_header_sector(primary_sb.value().primary_header_offset);
    if (!header_sector.is_ok()) {
        report.value().message = header_sector.message();
        return report;
    }

    const auto payload = keys.open_header(
        header_sector.value(),
        derived.value(),
        primary_sb.value(),
        container::HeaderSlotId::PrimaryOuter,
        container::HeaderSlotType::Outer);
    if (!payload.is_ok()) {
        report.value().message = payload.message();
        return report;
    }

    report.value().crypto_ok = true;
    report.value().message = "header decrypt ok";
    return report;
}

} 