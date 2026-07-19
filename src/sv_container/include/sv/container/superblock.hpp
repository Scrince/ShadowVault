#pragma once

#include <sv/container/types.hpp>
#include <sv/result.hpp>
#include <sv/span.hpp>

namespace sv::container {

[[nodiscard]] Result<void> encode_superblock(const Superblock& sb, Span<std::uint8_t> out) noexcept;
[[nodiscard]] Result<Superblock> decode_superblock(Span<const std::uint8_t> data) noexcept;
[[nodiscard]] Result<void> validate_superblock(const Superblock& sb) noexcept;
[[nodiscard]] std::uint32_t compute_superblock_crc(Span<const std::uint8_t> encoded_without_crc) noexcept;

} 