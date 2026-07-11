#pragma once

#include <sv/container/container_file.hpp>
#include <sv/container/types.hpp>
#include <sv/crypto/crypto_provider.hpp>
#include <sv/result.hpp>

#include <array>

namespace sv::recovery {

struct JournalSuperblock {
    std::array<std::uint8_t, 4> magic {'S', 'V', 'J', 'R'};
    std::uint32_t version = 1;
    std::uint64_t write_pointer = 0;
    std::uint64_t last_committed_seq = 0;
    std::uint64_t first_record_offset = 0;
    std::uint32_t crc32 = 0;
};

enum class JournalOpType : std::uint16_t {
    Begin = 1,
    ManifestWrite = 2,
    ChunkAlloc = 3,
    ChunkFree = 4,
    Commit = 5,
    Abort = 6,
};

class Journal {
public:
    Journal(
        container::ContainerFileStore& container,
        crypto::CryptoProvider& crypto,
        Span<const std::uint8_t> vmk,
        std::uint64_t journal_offset,
        std::uint64_t journal_size) noexcept;

    [[nodiscard]] Result<void> initialize() noexcept;
    [[nodiscard]] Result<void> append_begin(
        const std::array<std::uint8_t, 16>& transaction_id,
        container::Superblock* superblock) noexcept;
    [[nodiscard]] Result<void> append_commit(
        std::uint64_t manifest_generation,
        Span<const std::uint8_t> manifest_root_hash,
        container::Superblock* superblock) noexcept;
    [[nodiscard]] Result<void> append_abort(container::Superblock* superblock) noexcept;

    /// When force_check is true, scans the journal even if superblock has no dirty flag
    /// (required for hidden-volume journals, which do not set the outer dirty flag).
    [[nodiscard]] Result<void> replay_if_dirty(
        container::Superblock* superblock,
        bool force_check = false) noexcept;

private:
    [[nodiscard]] Result<JournalSuperblock> read_superblock() noexcept;
    [[nodiscard]] Result<void> write_superblock(const JournalSuperblock& sb) noexcept;
    [[nodiscard]] Result<void> write_container_superblock_flags(const container::Superblock& superblock) noexcept;
    [[nodiscard]] Result<std::uint64_t> append_record(
        JournalSuperblock& jsb,
        JournalOpType op_type,
        Span<const std::uint8_t> payload) noexcept;

    container::ContainerFileStore& container_;
    crypto::CryptoProvider& crypto_;
    Span<const std::uint8_t> vmk_;
    std::uint64_t journal_offset_ = 0;
    std::uint64_t journal_size_ = 0;
    std::uint64_t open_sequence_ = 0;
};

} // namespace sv::recovery