#pragma once

#include <sv/result.hpp>
#include <sv/span.hpp>

#include <cstdint>

namespace sv::crypto::detail {

[[nodiscard]] Result<void> lock_secure_region(Span<std::byte> region) noexcept;
[[nodiscard]] Result<void> unlock_secure_region(Span<std::byte> region) noexcept;
void wipe_secure_region(Span<std::uint8_t> region) noexcept;

} 