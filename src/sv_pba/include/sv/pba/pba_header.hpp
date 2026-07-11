#pragma once

#include <sv/result.hpp>
#include <sv/span.hpp>

#include <cstdint>

namespace sv::pba {

/// Inspected fields from a candidate 4096-byte system/container header sector.
struct SystemHeaderView {
    bool magic_ok = false;
    std::uint16_t format_version = 0;
};

/// Phase 1: validates envelope magic/version before KDF (freestanding-safe).
[[nodiscard]] SystemHeaderView inspect_header_sector(Span<const std::uint8_t> sector) noexcept;

} // namespace sv::pba