#pragma once

#include <sv/result.hpp>
#include <sv/span.hpp>

#include <cstdint>

namespace sv::crypto {

/// Fill buffer with cryptographically secure random bytes.
[[nodiscard]] Result<void> secure_random_bytes(Span<std::uint8_t> out) noexcept;

} // namespace sv::crypto