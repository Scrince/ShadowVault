#pragma once

#include <sv/container/types.hpp>
#include <sv/crypto/crypto_provider.hpp>
#include <sv/result.hpp>
#include <sv/vault/inode.hpp>

#include <array>
#include <vector>

namespace sv::vault {

constexpr std::size_t kManifestPageHeaderSize = 32;
constexpr std::size_t kManifestMaxPagePlaintext = 65536;

struct ManifestPageHeader {
    std::array<std::uint8_t, 4> page_magic {'S', 'V', 'M', 'P'};
    std::uint16_t page_version = 1;
    container::ManifestPageType page_type = container::ManifestPageType::InodeTable;
    std::uint64_t generation = 0;
    std::uint64_t page_index = 0;
    std::uint32_t entry_count = 0;
};

struct ManifestPage {
    ManifestPageHeader header;
    std::vector<std::uint8_t> body;
};

struct ManifestState {
    std::uint64_t generation = 0;
    std::vector<Inode> inodes;
    std::vector<std::uint8_t> bitmap;
    std::array<std::uint8_t, 32> root_hash {};
};

[[nodiscard]] Result<void> encode_manifest_page_header(
    const ManifestPageHeader& header,
    Span<std::uint8_t> out) noexcept;
[[nodiscard]] Result<ManifestPageHeader> decode_manifest_page_header(Span<const std::uint8_t> data) noexcept;

[[nodiscard]] std::size_t bitmap_byte_count(std::uint64_t data_size, std::uint32_t chunk_size) noexcept;
[[nodiscard]] std::uint64_t chunk_count_for_data(std::uint64_t data_size, std::uint32_t chunk_size) noexcept;

[[nodiscard]] Result<ManifestState> create_empty_manifest(std::uint64_t data_size, std::uint32_t chunk_size) noexcept;

[[nodiscard]] Result<std::array<std::uint8_t, 32>> compute_manifest_root_hash(
    const ManifestState& state) noexcept;

[[nodiscard]] Result<void> encrypt_manifest_page(
    crypto::CryptoProvider& crypto,
    Span<const std::uint8_t> vmk,
    const ManifestPage& page,
    Span<std::uint8_t> ciphertext_out,
    Span<std::uint8_t> tag_out) noexcept;

constexpr std::size_t kManifestPageSlotSize = kManifestMaxPagePlaintext + 16;

[[nodiscard]] Result<ManifestPage> decrypt_manifest_page(
    crypto::CryptoProvider& crypto,
    Span<const std::uint8_t> vmk,
    Span<const std::uint8_t> ciphertext,
    Span<const std::uint8_t> tag,
    std::uint64_t generation,
    std::uint64_t page_index) noexcept;

[[nodiscard]] Result<ManifestState> manifest_from_pages(
    const ManifestPage& inode_page,
    const ManifestPage& bitmap_page) noexcept;

[[nodiscard]] Result<std::pair<ManifestPage, ManifestPage>> manifest_to_pages(const ManifestState& state) noexcept;

} 