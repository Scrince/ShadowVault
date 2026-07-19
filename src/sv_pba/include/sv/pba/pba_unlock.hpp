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


struct PbaUnlockRequest {
    Span<const std::uint8_t> primary_header_sector;
    Span<const std::uint8_t> superblock_bytes;
    PbaCredentials credentials;
};


[[nodiscard]] Result<std::array<std::uint8_t, 32>> unlock_primary_header(
    const PbaUnlockRequest& request) noexcept;


[[nodiscard]] Result<std::array<std::uint8_t, 32>> unlock_system_header(
    Span<const std::uint8_t> primary_header_sector,
    const PbaCredentials& credentials) noexcept;

} 