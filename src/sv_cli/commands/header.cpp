#include <sv/cli/command_registry.hpp>
#include <sv/cli/vault_cmd.hpp>

#include <sv/container/container_file.hpp>
#include <sv/container/header_codec.hpp>
#include <sv/crc32.hpp>
#include <sv/endian.hpp>

#include <fstream>
#include <vector>

namespace sv::cli {

namespace {

constexpr std::size_t kBackupHeaderSize = 4 + 2 + 2 + 1 + 1 + 32 + 32 + container::SVLT_HEADER_SIZE + 4;

int backup_header(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    const auto positional = collect_positional_args(args);
    if (positional.size() < 2) {
        return output.error(ErrorCode::UsageError, "usage: sv backup-header VAULT_PATH OUTPUT.svhdr");
    }

    const std::filesystem::path vault_path = positional[0];
    const std::filesystem::path out_path = positional[1];

    auto container = container::ContainerFileStore::open(vault_path);
    if (!container.is_ok()) {
        return output.error(container.error(), container.message());
    }

    const auto sb = container.value().read_superblock_primary();
    if (!sb.is_ok()) {
        return output.error(sb.error(), sb.message());
    }

    const auto header = container.value().read_header_sector(sb.value().primary_header_offset);
    if (!header.is_ok()) {
        return output.error(header.error(), header.message());
    }

    std::array<std::uint8_t, container::SVLT_HEADER_SIZE> encoded {};
    const auto enc = container::encode_header_sector(header.value(), encoded);
    if (!enc.is_ok()) {
        return output.error(enc.error(), enc.message());
    }

    std::vector<std::uint8_t> backup(kBackupHeaderSize, 0);
    backup[0] = 'S';
    backup[1] = 'V';
    backup[2] = 'H';
    backup[3] = 'B';
    write_le16(backup.data() + 4, 1);
    write_le16(backup.data() + 6, static_cast<std::uint16_t>(container::HeaderSlotId::PrimaryOuter));
    backup[8] = static_cast<std::uint8_t>(container::HeaderSlotType::Outer);
    std::memcpy(backup.data() + 16, sb.value().outer_salt.data(), sb.value().outer_salt.size());
    std::memcpy(backup.data() + 48, sb.value().hidden_salt.data(), sb.value().hidden_salt.size());
    std::memcpy(backup.data() + 80, encoded.data(), encoded.size());
    const std::uint32_t crc = crc32_ieee(Span<const std::uint8_t>(backup.data(), backup.size() - 4));
    write_le32(backup.data() + backup.size() - 4, crc);

    std::ofstream out(out_path, std::ios::binary);
    if (!out) {
        return output.error(ErrorCode::IoError, "failed to open output file");
    }
    out.write(reinterpret_cast<const char*>(backup.data()), static_cast<std::streamsize>(backup.size()));
    return output.success("header backup written");
}

int restore_header(CliContext& ctx, const std::vector<std::string>& args) {
    Output output(ctx.globals);
    const auto positional = collect_positional_args(args);
    if (positional.size() < 2) {
        return output.error(ErrorCode::UsageError, "usage: sv restore-header VAULT_PATH INPUT.svhdr");
    }

    const std::filesystem::path vault_path = positional[0];
    const std::filesystem::path in_path = positional[1];

    std::ifstream in(in_path, std::ios::binary);
    if (!in) {
        return output.error(ErrorCode::IoError, "failed to open backup file");
    }

    std::vector<std::uint8_t> backup(kBackupHeaderSize);
    in.read(reinterpret_cast<char*>(backup.data()), static_cast<std::streamsize>(backup.size()));
    if (backup[0] != 'S' || backup[1] != 'V' || backup[2] != 'H' || backup[3] != 'B') {
        return output.error(ErrorCode::CorruptContainer, "invalid backup magic");
    }

    const std::uint32_t expected_crc = crc32_ieee(Span<const std::uint8_t>(backup.data(), backup.size() - 4));
    const std::uint32_t stored_crc = read_le32(backup.data() + backup.size() - 4);
    if (expected_crc != stored_crc) {
        return output.error(ErrorCode::CorruptContainer, "backup crc mismatch");
    }

    auto container = container::ContainerFileStore::open(vault_path);
    if (!container.is_ok()) {
        return output.error(container.error(), container.message());
    }

    const auto sb = container.value().read_superblock_primary();
    if (!sb.is_ok()) {
        return output.error(sb.error(), sb.message());
    }

    const auto sector = container::decode_header_sector(
        Span<const std::uint8_t>(backup.data() + 80, container::SVLT_HEADER_SIZE));
    if (!sector.is_ok()) {
        return output.error(sector.error(), sector.message());
    }

    const auto write_primary = container.value().write_header_sector(
        sb.value().primary_header_offset,
        sector.value());
    if (!write_primary.is_ok()) {
        return output.error(write_primary.error(), write_primary.message());
    }

    const auto write_backup = container.value().write_header_sector(
        sb.value().backup_header_offset,
        sector.value());
    if (!write_backup.is_ok()) {
        return output.error(write_backup.error(), write_backup.message());
    }

    (void)ctx;
    return output.success("header restored from backup");
}

} 

int backup_header_command(CliContext& ctx, const std::vector<std::string>& args) {
    return backup_header(ctx, args);
}

int restore_header_command(CliContext& ctx, const std::vector<std::string>& args) {
    return restore_header(ctx, args);
}

} 