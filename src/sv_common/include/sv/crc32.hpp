#pragma once

#include <sv/span.hpp>

#include <cstddef>
#include <cstdint>

namespace sv {

/// CRC32-IEEE (polynomial 0xEDB88320 reflected) for superblock and header envelopes.
[[nodiscard]] std::uint32_t crc32_ieee(Span<const std::uint8_t> data) noexcept;

} // namespace sv