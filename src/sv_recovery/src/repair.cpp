#include <sv/recovery/repair.hpp>

#include <sv/container/header_codec.hpp>
#include <sv/recovery/journal.hpp>
#include <sv/recovery/replay.hpp>

namespace sv::recovery {

namespace {

[[nodiscard]] bool primary_header_sector_valid(container::ContainerFileStore& container) noexcept {
    const auto primary_sb = container.read_superblock_primary();
    if (!primary_sb.is_ok()) {
        return false;
    }
    const auto primary = container.read_header_sector(primary_sb.value().primary_header_offset);
    return primary.is_ok();
}

} 

Result<void> repair_from_backup_header(
    container::ContainerFileStore& container,
    const RepairOptions options) noexcept {
    const auto primary_sb = container.read_superblock_primary();
    if (!primary_sb.is_ok()) {
        return Result<void>::err(primary_sb.error(), primary_sb.message());
    }

    const auto backup = container.read_header_sector(primary_sb.value().backup_header_offset);
    if (!backup.is_ok()) {
        return Result<void>::err(backup.error(), backup.message());
    }

    std::array<std::uint8_t, container::SVLT_HEADER_SIZE> encoded {};
    const auto enc = container::encode_header_sector(backup.value(), encoded);
    if (!enc.is_ok()) {
        return Result<void>::err(enc.error(), enc.message());
    }

    if (options.dry_run) {
        return Result<void>::ok();
    }

    const auto write = container.write_header_sector(primary_sb.value().primary_header_offset, backup.value());
    if (!write.is_ok()) {
        return write;
    }

    return container.sync();
}

Result<void> repair_replay_journal(
    container::ContainerFileStore& container,
    crypto::CryptoProvider& crypto,
    const Span<const std::uint8_t> vmk,
    container::Superblock& superblock,
    const RepairOptions options) noexcept {
    if ((superblock.flags & container::SUPERBLOCK_FLAG_JOURNAL_DIRTY) == 0) {
        return Result<void>::ok();
    }

    if (options.dry_run) {
        return Result<void>::ok();
    }

    Journal journal(
        container,
        crypto,
        vmk,
        superblock.journal_offset,
        superblock.journal_size);
    const auto replay = replay_journal(journal, superblock);
    if (!replay.is_ok()) {
        return replay;
    }

    const auto write_sb = container.write_superblock_both_no_sync(superblock);
    if (!write_sb.is_ok()) {
        return write_sb;
    }

    return container.sync();
}

Result<void> repair_vault(
    container::ContainerFileStore& container,
    crypto::CryptoProvider& crypto,
    const Span<const std::uint8_t> vmk,
    RepairOptions options) noexcept {
    auto primary_sb = container.read_superblock_primary();
    if (!primary_sb.is_ok()) {
        return Result<void>::err(primary_sb.error(), primary_sb.message());
    }

    if (options.replay_journal || (!options.from_backup && !options.replay_journal)) {
        const auto replay = repair_replay_journal(container, crypto, vmk, primary_sb.value(), options);
        if (!replay.is_ok()) {
            return replay;
        }
    }

    if (options.from_backup || (!options.from_backup && !options.replay_journal)) {
        if (!primary_header_sector_valid(container)) {
            return repair_from_backup_header(container, options);
        }
    }

    return Result<void>::ok();
}

} 