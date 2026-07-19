#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace sv::container {

constexpr std::uint8_t SVLT_MAGIC[4] = {'S', 'V', 'L', 'T'};
constexpr std::uint16_t SVLT_FORMAT_VERSION = 1;
constexpr std::size_t SVLT_SECTOR_SIZE = 512;
constexpr std::size_t SVLT_HEADER_SIZE = 4096;
constexpr std::size_t SVLT_HEADER_NONCE_SIZE = 24;
constexpr std::size_t SVLT_AAD_SIZE = 80;
constexpr std::size_t SVLT_HEADER_PAYLOAD_SIZE = 3552;
constexpr std::size_t SVLT_HEADER_CIPHERTEXT_SIZE = 3552;
constexpr std::uint32_t SVLT_DEFAULT_CHUNK_SIZE = 1024 * 1024;
constexpr std::size_t SVLT_INODE_SIZE = 340;
constexpr std::uint32_t SVLT_MAX_INODES_V1 = 192;
constexpr std::size_t SVLT_SUPERBLOCK_SIZE = 512;
constexpr std::size_t SVLT_SUPERBLOCK_CRC_END = 0xCC;
constexpr std::size_t SVLT_KDF_PARAMS_BLOB_SIZE = 64;

constexpr std::uint64_t SVLT_PRIMARY_HEADER_OFFSET = 0x200;
constexpr std::uint64_t SVLT_BACKUP_HEADER_OFFSET = 0x1200;
constexpr std::uint64_t SVLT_HIDDEN_HEADER_OFFSET = 0x2200;
constexpr std::uint64_t SVLT_JOURNAL_OFFSET = 0x3200;

constexpr std::uint32_t SUPERBLOCK_FLAG_CREATED = 1U << 0;
constexpr std::uint32_t SUPERBLOCK_FLAG_QUICK_FORMAT = 1U << 1;
constexpr std::uint32_t SUPERBLOCK_FLAG_HIDDEN_CONFIGURED = 1U << 2;
constexpr std::uint32_t SUPERBLOCK_FLAG_JOURNAL_DIRTY = 1U << 3;
constexpr std::uint32_t SUPERBLOCK_FLAG_KEYFILE_REQUIRED = 1U << 4;

enum class HeaderSlotType : std::uint8_t {
    Outer = 1,
    Hidden = 2,
};

enum class HeaderSlotId : std::uint16_t {
    PrimaryOuter = 1,
    BackupOuter = 2,
    Hidden = 3,
};

enum class InodeEntryType : std::uint8_t {
    File = 1,
    Directory = 2,
    Symlink = 3,
    HiddenReservation = 4,
};

enum class ManifestPageType : std::uint16_t {
    InodeTable = 1,
    FreeBitmap = 2,
};

struct Superblock {
    std::array<std::uint8_t, 4> magic{};
    std::uint16_t format_version = SVLT_FORMAT_VERSION;
    std::uint16_t superblock_size = static_cast<std::uint16_t>(SVLT_SUPERBLOCK_SIZE);
    std::uint32_t flags = 0;
    std::uint64_t container_size = 0;
    std::uint64_t primary_header_offset = SVLT_PRIMARY_HEADER_OFFSET;
    std::uint32_t primary_header_size = static_cast<std::uint32_t>(SVLT_HEADER_SIZE);
    std::uint64_t backup_header_offset = SVLT_BACKUP_HEADER_OFFSET;
    std::uint32_t backup_header_size = static_cast<std::uint32_t>(SVLT_HEADER_SIZE);
    std::uint64_t hidden_header_offset = SVLT_HIDDEN_HEADER_OFFSET;
    std::uint32_t hidden_header_size = static_cast<std::uint32_t>(SVLT_HEADER_SIZE);
    std::uint64_t journal_offset = SVLT_JOURNAL_OFFSET;
    std::uint64_t journal_size = 0;
    std::uint64_t manifest_offset = 0;
    std::uint64_t manifest_size = 0;
    std::uint64_t data_offset = 0;
    std::uint64_t data_size = 0;
    std::array<std::uint8_t, 32> outer_salt{};
    std::array<std::uint8_t, 32> hidden_salt{};
    std::array<std::uint8_t, 16> outer_kdf_id{};
    std::array<std::uint8_t, 16> hidden_kdf_id{};
    std::uint8_t deniability_profile = 0;
    std::array<std::uint8_t, 3> reserved_c{};
    std::uint32_t crc32 = 0;
};

struct HeaderPayload {
    std::uint16_t payload_version = 1;
    std::uint8_t slot_type = static_cast<std::uint8_t>(HeaderSlotType::Outer);
    std::uint8_t kdf_type = 1;
    std::array<std::uint8_t, 32> volume_master_key{};
    std::array<std::uint8_t, 32> header_key_confirm{};
    std::uint64_t manifest_offset = 0;
    std::uint64_t manifest_size = 0;
    std::uint64_t journal_offset = 0;
    std::uint64_t journal_size = 0;
    std::uint64_t data_offset = 0;
    std::uint64_t data_size = 0;
    std::uint32_t chunk_size = SVLT_DEFAULT_CHUNK_SIZE;
    std::uint8_t aead_id = 1;
    std::uint8_t keyfile_required = 0;
    std::array<std::uint8_t, SVLT_KDF_PARAMS_BLOB_SIZE> kdf_params_blob{};
    std::uint64_t manifest_generation = 0;
    std::uint64_t journal_sequence = 0;
    std::array<std::uint8_t, 32> manifest_root_hash{};
    std::uint64_t hidden_offset = 0;
    std::uint64_t hidden_max_size = 0;
    std::uint8_t hidden_present = 0;
    std::uint64_t protect_margin_bytes = 0;
    std::uint64_t hidden_journal_offset = 0;
    std::uint64_t hidden_journal_size = 0;
    std::uint64_t hidden_manifest_offset = 0;
    std::uint64_t hidden_manifest_size = 0;
    std::uint64_t hidden_data_offset = 0;
    std::uint64_t hidden_data_size = 0;
};

struct EncryptedHeaderSector {
    std::array<std::uint8_t, SVLT_HEADER_NONCE_SIZE> header_nonce{};
    std::array<std::uint8_t, SVLT_HEADER_CIPHERTEXT_SIZE> ciphertext{};
    std::array<std::uint8_t, 16> tag{};
    std::uint32_t header_slot_crc32 = 0;
};

struct ContainerLayout {
    std::uint64_t container_size = 0;
    std::uint64_t journal_offset = SVLT_JOURNAL_OFFSET;
    std::uint64_t journal_size = 0;
    std::uint64_t manifest_offset = 0;
    std::uint64_t manifest_size = 0;
    std::uint64_t data_offset = 0;
    std::uint64_t data_size = 0;
    std::uint64_t tail_superblock_offset = 0;
};

struct CreateLayoutParams {
    std::uint64_t container_size = 0;
    bool quick_format = false;
};

} 