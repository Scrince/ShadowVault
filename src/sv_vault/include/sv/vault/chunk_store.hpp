#pragma once

#include <sv/container/container_file.hpp>
#include <sv/crypto/crypto_provider.hpp>
#include <sv/result.hpp>
#include <sv/vault/inode.hpp>

namespace sv::vault {

struct ChunkWriteResult {
    std::uint32_t chunk_count = 0;
    std::uint64_t first_chunk_id = 0;
};

class ChunkStore {
public:
    ChunkStore(
        container::ContainerFileStore& container,
        crypto::CryptoProvider& crypto,
        std::uint64_t data_offset,
        std::uint32_t chunk_size) noexcept;

    [[nodiscard]] Result<std::vector<std::uint8_t>> read_file(
        const Inode& inode,
        Span<const std::uint8_t> vmk,
        std::uint64_t manifest_generation) const;

    [[nodiscard]] Result<ChunkWriteResult> write_file(
        const Inode& inode,
        Span<const std::uint8_t> vmk,
        std::uint64_t manifest_generation,
        Span<const std::uint8_t> data,
        std::uint64_t first_chunk_id);

    [[nodiscard]] std::uint64_t chunk_disk_offset(std::uint64_t chunk_id) const noexcept;

private:
    [[nodiscard]] Result<std::array<std::uint8_t, 24>> derive_chunk_nonce(
        Span<const std::uint8_t> base_nonce,
        std::uint32_t chunk_index) const noexcept;

    [[nodiscard]] Result<void> encrypt_chunk(
        Span<const std::uint8_t> vmk,
        std::uint64_t chunk_id,
        std::uint64_t inode_id,
        std::uint32_t chunk_index,
        std::uint64_t manifest_generation,
        Span<const std::uint8_t> base_nonce,
        Span<const std::uint8_t> plaintext,
        Span<std::uint8_t> ciphertext_out,
        Span<std::uint8_t> tag_out) const;

    [[nodiscard]] Result<std::vector<std::uint8_t>> decrypt_chunk(
        Span<const std::uint8_t> vmk,
        std::uint64_t chunk_id,
        std::uint64_t inode_id,
        std::uint32_t chunk_index,
        std::uint64_t manifest_generation,
        Span<const std::uint8_t> base_nonce,
        std::size_t plaintext_size,
        Span<const std::uint8_t> ciphertext,
        Span<const std::uint8_t> tag) const;

    container::ContainerFileStore& container_;
    crypto::CryptoProvider& crypto_;
    std::uint64_t data_offset_ = 0;
    std::uint32_t chunk_size_ = 0;
};

} 