#pragma once

#include <sv/span.hpp>

#include <cstdint>

namespace sv::crypto {

/// Constant-time byte comparison. Returns false when lengths differ.
[[nodiscard]] bool constant_time_compare(
    Span<const std::uint8_t> left,
    Span<const std::uint8_t> right) noexcept;

} // namespace sv::crypto