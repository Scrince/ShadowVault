#pragma once

#include <sv/pba/pba_header.hpp>
#include <sv/result.hpp>
#include <sv/span.hpp>

#include <array>
#include <cstdint>

namespace sv::pba {

struct PbaCredentials {
    Span<const std::uint8_t> password_utf8;
};

/// Inputs for host/lab unlock of a primary outer header (container-compatible envelope).
struct PbaUnlockRequest {
    Span<const std::uint8_t> primary_header_sector;
    Span<const std::uint8_t> superblock_bytes;
    PbaCredentials credentials;
};

/// Unwrap VMK from encrypted primary header + tail superblock (host Sprint A; EFI reuses API).
[[nodiscard]] Result<std::array<std::uint8_t, 32>> unlock_primary_header(
    const PbaUnlockRequest& request) noexcept;

/// Legacy entry — requires `superblock_bytes` inside request via `unlock_primary_header`.
[[nodiscard]] Result<std::array<std::uint8_t, 32>> unlock_system_header(
    Span<const std::uint8_t> primary_header_sector,
    const PbaCredentials& credentials) noexcept;

} // namespace sv::pba