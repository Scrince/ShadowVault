#include <sv/container/superblock.hpp>

#include <sv/container/layout.hpp>
#include <sv/crc32.hpp>
#include <sv/endian.hpp>

#include <cstring>

namespace sv::container {

namespace {

constexpr std::size_t kSuperblockCrcCoverage = 0xCC;

void write_bytes(std::uint8_t* out, std::size_t offset, Span<const std::uint8_t> bytes) noexcept {
    std::memcpy(out + offset, bytes.data(), bytes.size());
}

} 

std::uint32_t compute_superblock_crc(Span<const std::uint8_t> encoded_without_crc) noexcept {
    return crc32_ieee(encoded_without_crc.subspan(0, kSuperblockCrcCoverage));
}

Result<void> encode_superblock(const Superblock& sb, Span<std::uint8_t> out) noexcept {
    if (out.size() < SVLT_SUPERBLOCK_SIZE) {
        return Result<void>::err(ErrorCode::InvalidArgument, "superblock buffer too small");
    }

    std::memset(out.data(), 0, out.size());
    write_bytes(out.data(), 0x00, Span<const std::uint8_t>(sb.magic.data(), sb.magic.size()));
    write_le16(out.data() + 0x04, sb.format_version);
    write_le16(out.data() + 0x06, sb.superblock_size);
    write_le32(out.data() + 0x08, sb.flags);
    write_le64(out.data() + 0x0C, sb.container_size);
    write_le64(out.data() + 0x14, sb.primary_header_offset);
    write_le32(out.data() + 0x1C, sb.primary_header_size);
    write_le64(out.data() + 0x20, sb.backup_header_offset);
    write_le32(out.data() + 0x28, sb.backup_header_size);
    write_le64(out.data() + 0x2C, sb.hidden_header_offset);
    write_le32(out.data() + 0x34, sb.hidden_header_size);
    write_le64(out.data() + 0x38, sb.journal_offset);
    write_le64(out.data() + 0x40, sb.journal_size);
    write_le64(out.data() + 0x48, sb.manifest_offset);
    write_le64(out.data() + 0x50, sb.manifest_size);
    write_le64(out.data() + 0x58, sb.data_offset);
    write_le64(out.data() + 0x60, sb.data_size);
    write_bytes(out.data(), 0x68, Span<const std::uint8_t>(sb.outer_salt.data(), sb.outer_salt.size()));
    write_bytes(out.data(), 0x88, Span<const std::uint8_t>(sb.hidden_salt.data(), sb.hidden_salt.size()));
    write_bytes(out.data(), 0xA8, Span<const std::uint8_t>(sb.outer_kdf_id.data(), sb.outer_kdf_id.size()));
    write_bytes(out.data(), 0xB8, Span<const std::uint8_t>(sb.hidden_kdf_id.data(), sb.hidden_kdf_id.size()));
    out[0xC8] = sb.deniability_profile;
    write_bytes(out.data(), 0xC9, Span<const std::uint8_t>(sb.reserved_c.data(), sb.reserved_c.size()));

    const std::uint32_t crc = compute_superblock_crc(out);
    write_le32(out.data() + 0xCC, crc);
    return Result<void>::ok();
}

Result<Superblock> decode_superblock(Span<const std::uint8_t> data) noexcept {
    if (data.size() < SVLT_SUPERBLOCK_SIZE) {
        return Result<Superblock>::err(ErrorCode::CorruptContainer, "superblock too short");
    }

    Superblock sb {};
    std::memcpy(sb.magic.data(), data.data(), sb.magic.size());
    sb.format_version = read_le16(data.data() + 0x04);
    sb.superblock_size = read_le16(data.data() + 0x06);
    sb.flags = read_le32(data.data() + 0x08);
    sb.container_size = read_le64(data.data() + 0x0C);
    sb.primary_header_offset = read_le64(data.data() + 0x14);
    sb.primary_header_size = read_le32(data.data() + 0x1C);
    sb.backup_header_offset = read_le64(data.data() + 0x20);
    sb.backup_header_size = read_le32(data.data() + 0x28);
    sb.hidden_header_offset = read_le64(data.data() + 0x2C);
    sb.hidden_header_size = read_le32(data.data() + 0x34);
    sb.journal_offset = read_le64(data.data() + 0x38);
    sb.journal_size = read_le64(data.data() + 0x40);
    sb.manifest_offset = read_le64(data.data() + 0x48);
    sb.manifest_size = read_le64(data.data() + 0x50);
    sb.data_offset = read_le64(data.data() + 0x58);
    sb.data_size = read_le64(data.data() + 0x60);
    std::memcpy(sb.outer_salt.data(), data.data() + 0x68, sb.outer_salt.size());
    std::memcpy(sb.hidden_salt.data(), data.data() + 0x88, sb.hidden_salt.size());
    std::memcpy(sb.outer_kdf_id.data(), data.data() + 0xA8, sb.outer_kdf_id.size());
    std::memcpy(sb.hidden_kdf_id.data(), data.data() + 0xB8, sb.hidden_kdf_id.size());
    sb.deniability_profile = data[0xC8];
    std::memcpy(sb.reserved_c.data(), data.data() + 0xC9, sb.reserved_c.size());
    sb.crc32 = read_le32(data.data() + 0xCC);

    const auto validation = validate_superblock(sb);
    if (!validation.is_ok()) {
        return Result<Superblock>::err(validation.error(), validation.message());
    }

    const std::uint32_t expected_crc = compute_superblock_crc(data);
    if (sb.crc32 != expected_crc) {
        return Result<Superblock>::err(ErrorCode::CorruptContainer, "superblock crc mismatch");
    }

    return Result<Superblock>::ok(sb);
}

Result<void> validate_superblock(const Superblock& sb) noexcept {
    if (sb.magic[0] != SVLT_MAGIC[0] || sb.magic[1] != SVLT_MAGIC[1] || sb.magic[2] != SVLT_MAGIC[2]
        || sb.magic[3] != SVLT_MAGIC[3]) {
        return Result<void>::err(ErrorCode::CorruptContainer, "invalid superblock magic");
    }
    if (sb.format_version != SVLT_FORMAT_VERSION) {
        return Result<void>::err(ErrorCode::UnsupportedVersion, "unsupported format version");
    }
    if (sb.superblock_size != SVLT_SUPERBLOCK_SIZE) {
        return Result<void>::err(ErrorCode::CorruptContainer, "invalid superblock_size");
    }
    if (!validate_layout(sb)) {
        return Result<void>::err(ErrorCode::CorruptContainer, "superblock layout invalid");
    }
    return Result<void>::ok();
}

} 