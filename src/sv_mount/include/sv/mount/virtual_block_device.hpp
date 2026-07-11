#pragma once

#include <sv/mount/mount_types.hpp>
#include <sv/result.hpp>
#include <sv/span.hpp>
#include <sv/vault/chunk_store.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace sv::mount {

/// Exposes decrypted linear block address space backed by VaultEngine chunk_store.
class VirtualBlockDevice {
public:
    explicit VirtualBlockDevice(std::shared_ptr<vault::ChunkStore> chunk_store);

    [[nodiscard]] Result<std::size_t> read(std::uint64_t offset, Span<std::uint8_t> buf);
    [[nodiscard]] Result<std::size_t> write(std::uint64_t offset, Span<const std::uint8_t> buf);
    [[nodiscard]] Result<void> flush();
    [[nodiscard]] std::uint64_t size() const;
    [[nodiscard]] bool read_only() const;

private:
    [[nodiscard]] std::uint64_t block_index_for(std::uint64_t offset) const;
    [[nodiscard]] std::size_t block_offset(std::uint64_t offset) const;

    std::shared_ptr<vault::ChunkStore> chunk_store_;
};

} // namespace sv::mount