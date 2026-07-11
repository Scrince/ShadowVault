#pragma once

#include <sv/container/types.hpp>
#include <sv/result.hpp>
#include <sv/span.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace sv::vault {

struct Inode {
    std::uint16_t inode_version = 1;
    container::InodeEntryType entry_type = container::InodeEntryType::File;
    std::string name_utf8;
    std::uint64_t parent_id = 0;
    std::uint64_t inode_id = 0;
    std::uint64_t size = 0;
    std::uint64_t mtime_unix = 0;
    std::uint32_t chunk_count = 0;
    std::uint32_t flags = 0;
    std::uint64_t first_chunk_id = 0;
    std::array<std::uint8_t, 24> base_nonce {};
    /// Manifest generation used when chunk AEAD was written (required for multi-file reads).
    std::uint64_t data_generation = 0;
};

[[nodiscard]] Result<void> encode_inode(const Inode& inode, Span<std::uint8_t> out) noexcept;
[[nodiscard]] Result<Inode> decode_inode(Span<const std::uint8_t> data) noexcept;

[[nodiscard]] bool inode_less(const Inode& a, const Inode& b) noexcept;
[[nodiscard]] std::vector<Inode>::iterator find_inode(
    std::vector<Inode>& inodes,
    std::uint64_t parent_id,
    std::string_view name) noexcept;

[[nodiscard]] std::vector<Inode>::const_iterator find_inode(
    const std::vector<Inode>& inodes,
    std::uint64_t parent_id,
    std::string_view name) noexcept;

} // namespace sv::vault